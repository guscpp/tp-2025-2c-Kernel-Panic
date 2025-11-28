#!/bin/bash
# Script: tp_manager.sh
# Descripción: Gestor simple para tests del TP Master of Files

# Colores para output
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
IP_FILE="$BASE_DIR/dirs_ip.tmp"
LOGS_BASE_DIR="$BASE_DIR/logs_de_tests"
QUERIES_DIR="$BASE_DIR/queries"

# Función para crear directorio de logs
setup_logs_dir() {
    local test_num=$1
    local test_name=""
    
    case $test_num in
        1) test_name="test_1a-prueba_planificacion-fifo" ;;
        2) test_name="test_1b-prueba_planificacion-prioridades" ;;
        3) test_name="test_2a-prueba_memoria_worker-clock-m" ;;
        4) test_name="test_2b-prueba_memoria_worker-lru" ;;
        5) test_name="test_3-prueba_errores" ;;
        6) test_name="test_4a-prueba_storage" ;;
        7) test_name="test_4b-prueba_storage" ;;
        8) test_name="test_5-prueba_estabilidad_general" ;;
    esac
    
    LOG_DIR="$LOGS_BASE_DIR/$test_name"
    mkdir -p "$LOG_DIR"
    
    # Agregar separador en logs
    SEPARATOR="==============================================================================="
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
    LOG_HEADER="
$SEPARATOR
===  NUEVA EJECUCION: $TIMESTAMP                                   ===
$SEPARATOR"
    
    # Inicializar archivos de log con header
    case $test_num in
        1|2)
            for log in storage.log master.log worker1.log worker2.log query1.log query2.log query3.log query4.log; do
                echo -e "$LOG_HEADER" >> "$LOG_DIR/$log"
            done
            ;;
        3|4|5|6|7)
            for log in storage.log master.log worker.log query1.log; do
                echo -e "$LOG_HEADER" >> "$LOG_DIR/$log"
            done
            ;;
        8)
            for log in storage.log master.log worker1.log worker2.log worker3.log worker4.log worker5.log worker6.log query_batch.log; do
                echo -e "$LOG_HEADER" >> "$LOG_DIR/$log"
            done
            ;;
    esac
    
    print_info "Logs configurados en: $LOG_DIR"
}

# Función para cargar IPs desde archivo
load_ips() {
    if [ -f "$IP_FILE" ]; then
        source "$IP_FILE"
        return 0
    else
        return 1
    fi
}

# Función para guardar IPs en archivo
save_ips() {
    cat > "$IP_FILE" << EOF
MASTER_IP="$MASTER_IP"
STORAGE_IP="$STORAGE_IP"
WORKER_IP="$WORKER_IP"
QUERY_IP="$QUERY_IP"
EOF
    print_info "IPs guardadas en $IP_FILE"
}

# Función para preguntar IPs
ask_ips() {
    echo
    print_info "=== CONFIGURACIÓN DE DIRECCIONES IP ==="
    
    read -p "IP del Master (ej: 192.168.1.10): " MASTER_IP
    read -p "IP del Storage (ej: 192.168.1.11): " STORAGE_IP
    read -p "IP de los Workers (ej: 192.168.1.12): " WORKER_IP
    read -p "IP del Query Control (ej: 192.168.1.13): " QUERY_IP
    
    save_ips
}

# Función para mostrar IPs actuales
show_current_ips() {
    echo
    print_info "=== DIRECCIONES IP ACTUALES ==="
    echo "Master:      $MASTER_IP"
    echo "Storage:     $STORAGE_IP"
    echo "Workers:     $WORKER_IP"
    echo "Query:       $QUERY_IP"
    echo
}

# === GESTIÓN DE IPs ===
if load_ips; then
    print_info "IPs cargadas desde $IP_FILE"
    show_current_ips
    
    read -p "¿Usar estas IPs? (s/n/r=reingresar): " ip_choice
    case $ip_choice in
        [Ss]*) 
            print_info "Usando IPs existentes"
            ;;
        [Rr]*)
            ask_ips
            ;;
        *)
            print_info "Saliendo..."
            exit 0
            ;;
    esac
else
    print_warning "No se encontraron IPs guardadas"
    ask_ips
fi

# Puertos fijos
MASTER_PORT=9001
STORAGE_PORT=9002

# === MENÚ DE TESTS ===
echo
print_info "=== SELECCIÓN DE TEST ==="
echo "1) Test 1A - Planificación FIFO"
echo "2) Test 1B - Planificación PRIORIDADES" 
echo "3) Test 2A - Memoria Worker CLOCK-M"
echo "4) Test 2B - Memoria Worker LRU"
echo "5) Test 3 - Errores"
echo "6) Test 4A - Storage (primera mitad)"
echo "7) Test 4B - Storage (segunda mitad)"
echo "8) Test 5 - Estabilidad General"
read -p "Selecciona el test (1-8): " TEST_CHOICE

# Validar selección de test
if ! [[ "$TEST_CHOICE" =~ ^[1-8]$ ]]; then
    print_error "Selección de test inválida"
    exit 1
fi

# Preguntar demora para Test 5
if [ $TEST_CHOICE -eq 8 ]; then
    echo
    read -p "Ingresa la demora entre queries para Test 5 (segundos, ej: 1): " QUERY_DELAY
    if ! [[ "$QUERY_DELAY" =~ ^[0-9]+$ ]]; then
        QUERY_DELAY=1
        print_warning "Demora inválida, usando valor por defecto: 1 segundo"
    fi
    print_info "Demora configurada: $QUERY_DELAY segundos entre queries"
fi

# === MENÚ DE MÓDULOS ===
echo
print_info "=== SELECCIÓN DE MÓDULO ==="
echo "1) Master"
echo "2) Storage"
echo "3) Worker"
echo "4) Query Control"
read -p "Selecciona el módulo a ejecutar (1-4): " MODULE_CHOICE

# Validar selección de módulo
if ! [[ "$MODULE_CHOICE" =~ ^[1-4]$ ]]; then
    print_error "Selección de módulo inválida"
    exit 1
fi

# Configurar directorio de logs
setup_logs_dir $TEST_CHOICE

# Mostrar resumen de configuración
echo
print_info "=== RESUMEN DE CONFIGURACIÓN ==="
echo "Test seleccionado: $TEST_CHOICE"
echo "Módulo seleccionado: $MODULE_CHOICE"
if [ $TEST_CHOICE -eq 8 ]; then
    echo "Demora entre queries: $QUERY_DELAY segundos"
fi
show_current_ips
print_info "Directorio de logs: $LOG_DIR"

read -p "¿Continuar con esta configuración? (s/n): " confirm
if [[ ! $confirm =~ ^[Ss]$ ]]; then
    print_info "Operación cancelada"
    exit 0
fi

# Verificar binarios
print_info "Verificando binarios..."
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

# Array para guardar PIDs
declare -a PIDS=()
add_pid() { PIDS+=($1); }

# Función para configurar y ejecutar módulos
run_module() {
    local module=$1
    local test_num=$2
    
    case $module in
        "master")
            config_and_run_master $test_num
            ;;
        "storage")
            config_and_run_storage $test_num
            ;;
        "worker")
            config_and_run_worker $test_num
            ;;
        "query")
            config_and_run_query $test_num
            ;;
    esac
}

# Configuración y ejecución del Master
config_and_run_master() {
    local test_num=$1
    
    cd "$BASE_DIR/master" || { print_error "No se puede acceder a $BASE_DIR/master"; exit 1; }
    
    case $test_num in
        1) # Test 1A - FIFO
            cat > master.config << EOF
PUERTO_ESCUCHA=$MASTER_PORT
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=5000
LOG_LEVEL=INFO
EOF
            ;;
        2) # Test 1B - PRIORIDADES
            cat > master.config << EOF
PUERTO_ESCUCHA=$MASTER_PORT
ALGORITMO_PLANIFICACION=PRIORIDADES
TIEMPO_AGING=5000
LOG_LEVEL=INFO
EOF
            ;;
        3|4) # Tests 2A, 2B - FIFO
            cat > master.config << EOF
PUERTO_ESCUCHA=$MASTER_PORT
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=0
LOG_LEVEL=INFO
EOF
            ;;
        5) # Test 3 - FIFO
            cat > master.config << EOF
PUERTO_ESCUCHA=$MASTER_PORT
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=0
LOG_LEVEL=INFO
EOF
            ;;
        6|7) # Tests 4A, 4B - PRIORIDADES
            cat > master.config << EOF
PUERTO_ESCUCHA=$MASTER_PORT
ALGORITMO_PLANIFICACION=PRIORIDADES
TIEMPO_AGING=0
LOG_LEVEL=INFO
EOF
            ;;
        8) # Test 5 - PRIORIDADES
            cat > master.config << EOF
PUERTO_ESCUCHA=$MASTER_PORT
ALGORITMO_PLANIFICACION=PRIORIDADES
TIEMPO_AGING=250
LOG_LEVEL=INFO
EOF
            ;;
    esac
    
    print_info "Iniciando Master desde su carpeta..."
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
}

# Configuración y ejecución del Storage
config_and_run_storage() {
    local test_num=$1
    
    cd "$BASE_DIR/storage" || { print_error "No se puede acceder a $BASE_DIR/storage"; exit 1; }
    
    # Configurar superblock según test
    case $test_num in
        1|2) # Tests 1A, 1B
            cat > superblock.config << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
            FRESH_START="TRUE"
            RETARDO="250"
            ;;
        *) # Otros tests
            cat > superblock.config << EOF
BLOCK_SIZE=16
FS_SIZE=4096
EOF
            FRESH_START="FALSE"
            RETARDO="500"
            ;;
    esac
    
    # Test 5 tiene retardos diferentes
    if [ $test_num -eq 8 ]; then
        RETARDO="100"
    fi
    
    cat > storage.config << EOF
PUERTO_ESCUCHA=$STORAGE_PORT
FRESH_START=$FRESH_START
PUNTO_MONTAJE=$BASE_DIR/storage
RETARDO_OPERACION=$RETARDO
RETARDO_ACCESO_BLOQUE=$RETARDO
LOG_LEVEL=INFO
EOF
    
    print_info "Iniciando Storage desde su carpeta..."
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
}

# Configuración y ejecución de Workers
config_and_run_worker() {
    local test_num=$1
    
    cd "$BASE_DIR/worker" || { print_error "No se puede acceder a $BASE_DIR/worker"; exit 1; }
    
    case $test_num in
        1) # Test 1A - 2 Workers
            print_info "Iniciando Workers desde su carpeta..."
            
            # Worker 1
            cat > worker1.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=1024
RETARDO_MEMORIA=500
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            print_info "Iniciando Worker 1 con worker1.config..."
            ./bin/worker worker1.config 1 >> "$LOG_DIR/worker1.log" 2>&1 &
            WORKER1_PID=$!
            add_pid $WORKER1_PID
            
            # Espera breve para evitar race condition
            sleep 1
            
            # Worker 2
            cat > worker2.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=1024
RETARDO_MEMORIA=500
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
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
            ;;
            
        2) # Test 1B - 2 Workers (misma config que 1A)
            config_and_run_worker 1
            ;;
            
        3) # Test 2A - 1 Worker CLOCK-M
            print_info "Iniciando Worker desde su carpeta..."
            cat > worker.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=48
RETARDO_MEMORIA=250
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker.config 1 >> "$LOG_DIR/worker.log" 2>&1 &
            WORKER_PID=$!
            add_pid $WORKER_PID
            wait_with_message 5 "Esperando que Worker esté listo"
            if is_process_running "worker"; then
                print_info "Worker iniciado (PID: $WORKER_PID)"
            else
                print_error "Error al iniciar Worker. Revisar $LOG_DIR/worker.log"
                exit 1
            fi
            ;;
            
        4) # Test 2B - 1 Worker LRU
            print_info "Iniciando Worker desde su carpeta..."
            cat > worker.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=48
RETARDO_MEMORIA=250
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker.config 1 >> "$LOG_DIR/worker.log" 2>&1 &
            WORKER_PID=$!
            add_pid $WORKER_PID
            wait_with_message 5 "Esperando que Worker esté listo"
            if is_process_running "worker"; then
                print_info "Worker iniciado (PID: $WORKER_PID)"
            else
                print_error "Error al iniciar Worker. Revisar $LOG_DIR/worker.log"
                exit 1
            fi
            ;;
            
        5) # Test 3 - 1 Worker
            print_info "Iniciando Worker desde su carpeta..."
            cat > worker.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=256
RETARDO_MEMORIA=250
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker.config 1 >> "$LOG_DIR/worker.log" 2>&1 &
            WORKER_PID=$!
            add_pid $WORKER_PID
            wait_with_message 5 "Esperando que Worker esté listo"
            if is_process_running "worker"; then
                print_info "Worker iniciado (PID: $WORKER_PID)"
            else
                print_error "Error al iniciar Worker. Revisar $LOG_DIR/worker.log"
                exit 1
            fi
            ;;
            
        6|7) # Tests 4A, 4B - 1 Worker
            print_info "Iniciando Worker desde su carpeta..."
            cat > worker.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=128
RETARDO_MEMORIA=250
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker.config 1 >> "$LOG_DIR/worker.log" 2>&1 &
            WORKER_PID=$!
            add_pid $WORKER_PID
            wait_with_message 5 "Esperando que Worker esté listo"
            if is_process_running "worker"; then
                print_info "Worker iniciado (PID: $WORKER_PID)"
            else
                print_error "Error al iniciar Worker. Revisar $LOG_DIR/worker.log"
                exit 1
            fi
            ;;
            
        8) # Test 5 - Múltiples Workers
            print_info "Iniciando Workers desde su carpeta..."
            
            # Worker 1
            cat > worker1.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=128
RETARDO_MEMORIA=100
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker1.config 1 >> "$LOG_DIR/worker1.log" 2>&1 &
            add_pid $!
            print_info "Worker 1 iniciado"
            
            # Worker 2
            cat > worker2.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=256
RETARDO_MEMORIA=50
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker2.config 2 >> "$LOG_DIR/worker2.log" 2>&1 &
            add_pid $!
            print_info "Worker 2 iniciado"
            
            # Worker 3
            cat > worker3.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=64
RETARDO_MEMORIA=150
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker3.config 3 >> "$LOG_DIR/worker3.log" 2>&1 &
            add_pid $!
            print_info "Worker 3 iniciado"
            
            # Worker 4
            cat > worker4.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=96
RETARDO_MEMORIA=125
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker4.config 4 >> "$LOG_DIR/worker4.log" 2>&1 &
            add_pid $!
            print_info "Worker 4 iniciado"
            
            # Worker 5
            cat > worker5.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=160
RETARDO_MEMORIA=75
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker5.config 5 >> "$LOG_DIR/worker5.log" 2>&1 &
            add_pid $!
            print_info "Worker 5 iniciado"
            
            # Worker 6
            cat > worker6.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=192
RETARDO_MEMORIA=175
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=INFO
EOF
            ./bin/worker worker6.config 6 >> "$LOG_DIR/worker6.log" 2>&1 &
            add_pid $!
            print_info "Worker 6 iniciado"
            
            wait_with_message 5 "Esperando que Workers estén listos"
            print_info "Todos los Workers iniciados"
            ;;
    esac
}

# Configuración y ejecución de Query Control
config_and_run_query() {
    local test_num=$1
    
    cd "$BASE_DIR/query_control" || { print_error "No se puede acceder a $BASE_DIR/query_control"; exit 1; }
    
    cat > query_control.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
LOG_LEVEL=INFO
EOF
    
    case $test_num in
        1) # Test 1A - FIFO
            print_info "Ejecutando Queries FIFO..."
            
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
            ;;
        2) # Test 1B - PRIORIDADES
            print_info "Ejecutando Queries AGING..."
            
            ./bin/query_control query_control.config "$QUERIES_DIR/AGING_1" 4 >> "$LOG_DIR/query1.log" 2>&1 &
            QUERY1_PID=$!
            add_pid $QUERY1_PID
            wait_with_message 2 "Entre queries"
            
            ./bin/query_control query_control.config "$QUERIES_DIR/AGING_2" 3 >> "$LOG_DIR/query2.log" 2>&1 &
            QUERY2_PID=$!
            add_pid $QUERY2_PID
            wait_with_message 2 "Entre queries"
            
            ./bin/query_control query_control.config "$QUERIES_DIR/AGING_3" 5 >> "$LOG_DIR/query3.log" 2>&1 &
            QUERY3_PID=$!
            add_pid $QUERY3_PID
            wait_with_message 2 "Entre queries"
            
            ./bin/query_control query_control.config "$QUERIES_DIR/AGING_4" 1 >> "$LOG_DIR/query4.log" 2>&1 &
            QUERY4_PID=$!
            add_pid $QUERY4_PID
            
            print_info "Todas las queries AGING enviadas"
            ;;
        3) # Test 2A - CLOCK-M
            print_info "Ejecutando Query MEMORIA_WORKER..."
            ./bin/query_control query_control.config "$QUERIES_DIR/MEMORIA_WORKER" 0 >> "$LOG_DIR/query1.log" 2>&1 &
            QUERY1_PID=$!
            add_pid $QUERY1_PID
            print_info "Query MEMORIA_WORKER enviada"
            ;;
        4) # Test 2B - LRU
            print_info "Ejecutando Query MEMORIA_WORKER_2..."
            ./bin/query_control query_control.config "$QUERIES_DIR/MEMORIA_WORKER_2" 0 >> "$LOG_DIR/query1.log" 2>&1 &
            QUERY1_PID=$!
            add_pid $QUERY1_PID
            print_info "Query MEMORIA_WORKER_2 enviada"
            ;;
        5) # Test 3 - Errores
            print_info "Ejecutando Queries de Errores..."
            
            ./bin/query_control query_control.config "$QUERIES_DIR/ESCRITURA_ARCHIVO_COMMITED" 1 >> "$LOG_DIR/query1.log" 2>&1 &
            QUERY1_PID=$!
            add_pid $QUERY1_PID
            wait_with_message 2 "Entre queries"
            
            ./bin/query_control query_control.config "$QUERIES_DIR/FILE_EXISTENTE" 1 >> "$LOG_DIR/query2.log" 2>&1 &
            QUERY2_PID=$!
            add_pid $QUERY2_PID
            wait_with_message 2 "Entre queries"
            
            ./bin/query_control query_control.config "$QUERIES_DIR/LECTURA_FUERA_DEL_LIMITE" 1 >> "$LOG_DIR/query3.log" 2>&1 &
            QUERY3_PID=$!
            add_pid $QUERY3_PID
            wait_with_message 2 "Entre queries"
            
            ./bin/query_control query_control.config "$QUERIES_DIR/TAG_EXISTENTE" 1 >> "$LOG_DIR/query4.log" 2>&1 &
            QUERY4_PID=$!
            add_pid $QUERY4_PID
            
            print_info "Todas las queries de errores enviadas"
            ;;
        6) # Test 4A - Storage primera mitad
            print_info "Ejecutando Queries STORAGE 1-4..."
            
            ./bin/query_control query_control.config "$QUERIES_DIR/STORAGE_1" 0 >> "$LOG_DIR/query1.log" 2>&1 &
            QUERY1_PID=$!
            add_pid $QUERY1_PID
            wait_with_message 2 "Entre queries"
            
            ./bin/query_control query_control.config "$QUERIES_DIR/STORAGE_2" 2 >> "$LOG_DIR/query2.log" 2>&1 &
            QUERY2_PID=$!
            add_pid $QUERY2_PID
            wait_with_message 2 "Entre queries"
            
            ./bin/query_control query_control.config "$QUERIES_DIR/STORAGE_3" 4 >> "$LOG_DIR/query3.log" 2>&1 &
            QUERY3_PID=$!
            add_pid $QUERY3_PID
            wait_with_message 2 "Entre queries"
            
            ./bin/query_control query_control.config "$QUERIES_DIR/STORAGE_4" 6 >> "$LOG_DIR/query4.log" 2>&1 &
            QUERY4_PID=$!
            add_pid $QUERY4_PID
            
            print_info "Todas las queries STORAGE 1-4 enviadas"
            ;;
        7) # Test 4B - Storage segunda mitad
            print_info "Ejecutando Query STORAGE_5..."
            ./bin/query_control query_control.config "$QUERIES_DIR/STORAGE_5" 0 >> "$LOG_DIR/query1.log" 2>&1 &
            QUERY1_PID=$!
            add_pid $QUERY1_PID
            print_info "Query STORAGE_5 enviada"
            ;;
        8) # Test 5 - Estabilidad
            print_info "Ejecutando 100 queries AGING con demora de $QUERY_DELAY segundos..."
            for i in {1..25}; do
                print_info "Lote $i/25 - Ejecutando 4 queries..."
                ./bin/query_control query_control.config "$QUERIES_DIR/AGING_1" 20 >> "$LOG_DIR/query_batch.log" 2>&1 &
                add_pid $!
                sleep $QUERY_DELAY
                ./bin/query_control query_control.config "$QUERIES_DIR/AGING_2" 20 >> "$LOG_DIR/query_batch.log" 2>&1 &
                add_pid $!
                sleep $QUERY_DELAY
                ./bin/query_control query_control.config "$QUERIES_DIR/AGING_3" 20 >> "$LOG_DIR/query_batch.log" 2>&1 &
                add_pid $!
                sleep $QUERY_DELAY
                ./bin/query_control query_control.config "$QUERIES_DIR/AGING_4" 20 >> "$LOG_DIR/query_batch.log" 2>&1 &
                add_pid $!
                sleep $QUERY_DELAY
            done
            print_info "Todas las queries lanzadas. Ejecutándose en segundo plano..."
            print_info "Puedes monitorear el progreso en: $LOG_DIR/query_batch.log"
            ;;
    esac
}

# === EJECUCIÓN PRINCIPAL ===
echo
print_info "Iniciando configuración..."

case $MODULE_CHOICE in
    1) run_module "master" $TEST_CHOICE ;;
    2) run_module "storage" $TEST_CHOICE ;;
    3) run_module "worker" $TEST_CHOICE ;;
    4) run_module "query" $TEST_CHOICE ;;
esac

print_info "=== CONFIGURACIÓN COMPLETADA ==="
print_info "Test: $TEST_CHOICE"
print_info "Módulo: $MODULE_CHOICE" 
print_info "IPs guardadas en: $IP_FILE"
print_info "Logs disponibles en: $LOG_DIR"
print_info ""
print_info "Módulos ejecutándose en segundo plano."
print_info "Presiona Ctrl+C para finalizar todos los procesos."

# Mantener script vivo para que los procesos sigan ejecutándose
while true; do
    sleep 10
done