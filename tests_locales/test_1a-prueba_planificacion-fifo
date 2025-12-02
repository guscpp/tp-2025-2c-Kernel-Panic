#!/bin/bash
# Script: test_1a-prueba_planificacion-fifo.sh
# Descripción: Prueba Planificación - Algoritmo FIFO
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
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(dirname "$SCRIPT_DIR")"
QUERIES_DIR="$BASE_DIR/queries"
TEST_NAME="test_1a-prueba_planificacion-fifo"
LOG_DIR="$SCRIPT_DIR/logs/$TEST_NAME"
mkdir -p "$LOG_DIR"
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
bins=(
    "$BASE_DIR/storage/bin/storage"
    "$BASE_DIR/master/bin/master"
    "$BASE_DIR/worker/bin/worker"
    "$BASE_DIR/query_control/bin/query_control"
)
for bin in "${bins[@]}"; do
    [ -f "$bin" ] || { print_error "Binario no encontrado: $bin"; exit 1; }
done
# === CONFIGURAR .config LOCALES ===
cat > "$BASE_DIR/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=0
LOG_LEVEL=DEBUG
EOF
cat > "$BASE_DIR/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=TRUE
PUNTO_MONTAJE=$BASE_DIR/storage
RETARDO_OPERACION=25
RETARDO_ACCESO_BLOQUE=25
LOG_LEVEL=DEBUG
EOF
cat > "$BASE_DIR/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
cat > "$BASE_DIR/worker/worker1.config" << EOF
IP_MASTER=127.0.0.1
PUERTO_MASTER=9001
IP_STORAGE=127.0.0.1
PUERTO_STORAGE=9002
TAM_MEMORIA=1024
RETARDO_MEMORIA=50
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
RETARDO_MEMORIA=50
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=DEBUG
EOF
cat > "$BASE_DIR/query_control/query_control.config" << EOF
IP_MASTER=127.0.0.1
PUERTO_MASTER=9001
LOG_LEVEL=DEBUG
EOF
# === INICIAR MÓDULOS ===
declare -a PIDS=()
add_pid() { PIDS+=($1); }
print_info "Iniciando Storage..."
cd "$BASE_DIR/storage"
./bin/storage storage.config >> "$LOG_DIR/storage.log" 2>&1 &
STORAGE_PID=$!
add_pid $STORAGE_PID
wait_with_message 5 "Esperando que Storage esté listo"
is_process_running "storage" || { print_error "Storage falló"; exit 1; }
print_info "Iniciando Master..."
cd "$BASE_DIR/master"
./bin/master master.config >> "$LOG_DIR/master.log" 2>&1 &
MASTER_PID=$!
add_pid $MASTER_PID
wait_with_message 5 "Esperando que Master esté listo"
is_process_running "master" || { print_error "Master falló"; exit 1; }
print_info "Iniciando Workers..."
cd "$BASE_DIR/worker"
./bin/worker worker1.config 1 >> "$LOG_DIR/worker1.log" 2>&1 &
WORKER1_PID=$!
add_pid $WORKER1_PID
sleep 1
./bin/worker worker2.config 2 >> "$LOG_DIR/worker2.log" 2>&1 &
WORKER2_PID=$!
add_pid $WORKER2_PID
wait_with_message 5 "Esperando que Workers estén listos"
[ $(pgrep -f "worker" | wc -l) -ge 2 ] || { print_error "Workers fallaron"; exit 1; }
# === EJECUTAR QUERIES ===
cd "$BASE_DIR/query_control"
./bin/query_control query_control.config "$QUERIES_DIR/FIFO_1" 4 >> "$LOG_DIR/query1.log" 2>&1 &
add_pid $!
wait_with_message 2 "Entre queries"
./bin/query_control query_control.config "$QUERIES_DIR/FIFO_2" 3 >> "$LOG_DIR/query2.log" 2>&1 &
add_pid $!
wait_with_message 2 "Entre queries"
./bin/query_control query_control.config "$QUERIES_DIR/FIFO_3" 5 >> "$LOG_DIR/query3.log" 2>&1 &
add_pid $!
wait_with_message 2 "Entre queries"
./bin/query_control query_control.config "$QUERIES_DIR/FIFO_4" 1 >> "$LOG_DIR/query4.log" 2>&1 &
add_pid $!
print_info "Todas las queries FIFO enviadas"
# === MONITOREO ===
for i in {1..12}; do
    sleep 5
    q=$(pgrep -f "query_control" | wc -l)
    [ $q -eq 0 ] && { print_info "Todas las queries finalizaron"; break; }
done
print_info "=== TEST FIFO COMPLETADO ==="
print_info "Presiona Ctrl+C para finalizar."
while true; do sleep 10; done