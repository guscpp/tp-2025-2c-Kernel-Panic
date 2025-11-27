#!/bin/bash
# Script: test_1a-prueba_planificacion-fifo.sh
# Descripción: Automatiza la ejecución del test "Prueba Planificación" - 1A FIFO
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

wait_with_message() {
    local seconds=$1
    local message=$2
    print_info "$message - Esperando $seconds segundos..."
    sleep $seconds
}

is_process_running() {
    pgrep -f "$1" > /dev/null 2>&1
}

kill_tp_processes() {
    print_info "Finalizando procesos del TP..."
    pkill -f "./bin/storage" 2>/dev/null
    pkill -f "./bin/master" 2>/dev/null
    pkill -f "./bin/worker" 2>/dev/null
    pkill -f "./bin/query_control" 2>/dev/null
    sleep 2
}

safe_cleanup() {
    echo
    print_info "Ctrl+C recibido. Limpiando..."
    kill_tp_processes
    print_info "Limpieza completada. Saliendo..."
    exit 0
}

trap safe_cleanup INT TERM

BASE_DIR="$(pwd)"
#echo -e "$BASE_DIR"
QUERIES_DIR="$BASE_DIR/queries"
#echo -e "$QUERIES_DIR"
TEST_NAME="test_1a-prueba_planificacion-fifo"
LOG_DIR="$BASE_DIR/logs_de_tests/$TEST_NAME"

mkdir -p "$LOG_DIR"

# Agregar separador en logs
SEPARATOR="==============================================================================="
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
LOG_HEADER="
$SEPARATOR
===  NUEVA EJECUCION: $TIMESTAMP                                   ===
$SEPARATOR"

for log in storage.log master.log worker1.log worker2.log query{1..4}.log; do
    echo -e "$LOG_HEADER" >> "$LOG_DIR/$log"
done

print_info "Iniciando Test FIFO - Prueba Planificación"
print_info "Directorio base: $BASE_DIR"

# Verificar binarios
bins=(
    "$BASE_DIR/storage/bin/storage"
    "$BASE_DIR/master/bin/master"
    "$BASE_DIR/worker/bin/worker"
    "$BASE_DIR/query_control/bin/query_control"
)
for bin in "${bins[@]}"; do
    [ -f "$bin" ] || { print_error "Binario no encontrado: $bin"; exit 1; }
done
print_info "Todos los binarios encontrados"

# === CONFIGURAR .config LOCALES ===
print_info "Configurando master/master.config para FIFO..."
cat > "$BASE_DIR/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=5000
LOG_LEVEL=DEBUG
EOF

print_info "Configurando storage/storage.config para FIFO (FRESH_START=TRUE)..."
cat > "$BASE_DIR/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=TRUE
PUNTO_MONTAJE=$BASE_DIR/storage
RETARDO_OPERACION=250
RETARDO_ACCESO_BLOQUE=250
LOG_LEVEL=DEBUG
EOF

# Asegurar superblock.config en storage/
cat > "$BASE_DIR/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
print_info "superblock.config creado en storage/"

print_info "Configurando worker/worker1.config y worker2.config..."
cat > "$BASE_DIR/worker/worker1.config" << EOF
IP_MASTER=127.0.0.1
PUERTO_MASTER=9001
IP_STORAGE=127.0.0.1
PUERTO_STORAGE=9002
TAM_MEMORIA=1024
RETARDO_MEMORIA=500
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=DEBUG
EOF

cat > "$BASE_DIR/worker/worker2.config" << EOF
IP_MASTER=127.0.0.1
PUERTO_MASTER=9001
IP_STORAGE=127.0.0.1
PUERTO_STORAGE=9002
TAM_MEMORIA=1024
RETARDO_MEMORIA=500
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=DEBUG
EOF

# Configurar query_control.config
cat > "$BASE_DIR/query_control/query_control.config" << EOF
IP_MASTER=127.0.0.1
PUERTO_MASTER=9001
LOG_LEVEL=DEBUG
EOF
print_info "query_control.config configurado"

# === INICIAR MÓDULOS ===
declare -a PIDS=()
add_pid() { PIDS+=($1); }

# Storage
print_info "Iniciando Storage desde su carpeta..."
cd "$BASE_DIR/storage"
./bin/storage storage.config >> "$LOG_DIR/storage.log" 2>&1 &
STORAGE_PID=$!
add_pid $STORAGE_PID
wait_with_message 5 "Esperando que Storage esté listo"
if is_process_running "storage"; then
    print_info "Storage iniciado (PID: $STORAGE_PID)"
else
    print_error "Error al iniciar Storage. Revisar $LOG_DIR/storage.log"
    exit 1
fi

# Master
print_info "Iniciando Master desde su carpeta..."
cd "$BASE_DIR/master"
./bin/master master.config >> "$LOG_DIR/master.log" 2>&1 &
MASTER_PID=$!
add_pid $MASTER_PID
wait_with_message 5 "Esperando que Master esté listo"
if is_process_running "master"; then
    print_info "Master iniciado (PID: $MASTER_PID)"
else
    print_error "Error al iniciar Master. Revisar $LOG_DIR/master.log"
    exit 1
fi

# Workers: lanzar con sus propios archivos de config, SIN sobrescribir worker.config
print_info "Iniciando Workers desde su carpeta..."
cd "$BASE_DIR/worker"

# Worker 1 - usar config directamente
print_info "Iniciando Worker 1 con worker1.config..."
./bin/worker worker1.config 1 >> "$LOG_DIR/worker1.log" 2>&1 &
WORKER1_PID=$!
add_pid $WORKER1_PID

# Espera breve para evitar race condition
sleep 1

# Worker 2 - usar config directamente  
print_info "Iniciando Worker 2 con worker2.config..."
./bin/worker worker2.config 2 >> "$LOG_DIR/worker2.log" 2>&1 &
WORKER2_PID=$!
add_pid $WORKER2_PID

wait_with_message 5 "Esperando que Workers estén listos"
if [ $(pgrep -f "worker" | wc -l) -ge 2 ]; then
    print_info "Workers iniciados (PIDs: $WORKER1_PID, $WORKER2_PID)"
else
    print_error "Error al iniciar Workers. Revisar logs."
    exit 1
fi

# === EJECUTAR QUERIES ===
print_info "Ejecutando Queries FIFO..."
cd "$BASE_DIR/query_control"

./bin/query_control query_control.config "$QUERIES_DIR/FIFO_1" 4 >> "$LOG_DIR/query1.log" 2>&1 &
QUERY1_PID=$!
add_pid $QUERY1_PID
wait_with_message 2 "Entre queries"

./bin/query_control query_control.config "$QUERIES_DIR/FIFO_2" 3 >> "$LOG_DIR/query2.log" 2>&1 &
QUERY2_PID=$!
add_pid $QUERY2_PID
wait_with_message 2 "Entre queries"

./bin/query_control query_control.config "$QUERIES_DIR/FIFO_3" 5 >> "$LOG_DIR/query3.log" 2>&1 &
QUERY3_PID=$!
add_pid $QUERY3_PID
wait_with_message 2 "Entre queries"

./bin/query_control query_control.config "$QUERIES_DIR/FIFO_4" 1 >> "$LOG_DIR/query4.log" 2>&1 &
QUERY4_PID=$!
add_pid $QUERY4_PID

print_info "Todas las queries FIFO enviadas"

# === MONITOREO ===
print_info "Monitoreando ejecución... Logs en: $LOG_DIR/"
for i in {1..12}; do
    sleep 5
    q=$(pgrep -f "query_control" | wc -l)
    print_info "Queries en ejecución: $q"
    [ $q -eq 0 ] && { print_info "Todas las queries finalizaron"; break; }
done

print_info "=== TEST FIFO COMPLETADO ==="
print_info "El filesystem está listo en $BASE_DIR/storage para el test de PRIORIDADES."
print_info "Para el siguiente test, asegúrate de usar FRESH_START=FALSE."
print_info "Presiona Ctrl+C para finalizar los módulos."
while true; do sleep 10; done
