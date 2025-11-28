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

BASE_DIR="$(pwd)"
IP_FILE="$BASE_DIR/dirs_ip.tmp"

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

# Mostrar resumen de configuración
echo
print_info "=== RESUMEN DE CONFIGURACIÓN ==="
echo "Test seleccionado: $TEST_CHOICE"
echo "Módulo seleccionado: $MODULE_CHOICE"
show_current_ips

read -p "¿Continuar con esta configuración? (s/n): " confirm
if [[ ! $confirm =~ ^[Ss]$ ]]; then
    print_info "Operación cancelada"
    exit 0
fi

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
    
    print_info "Ejecutando Master..."
    ./bin/master master.config &
    MASTER_PID=$!
    print_info "Master iniciado (PID: $MASTER_PID)"
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
    
    print_info "Ejecutando Storage..."
    ./bin/storage storage.config &
    STORAGE_PID=$!
    print_info "Storage iniciado (PID: $STORAGE_PID)"
}

# Configuración y ejecución de Workers
config_and_run_worker() {
    local test_num=$1
    
    cd "$BASE_DIR/worker" || { print_error "No se puede acceder a $BASE_DIR/worker"; exit 1; }
    
    case $test_num in
        1) # Test 1A - 2 Workers
            print_info "Iniciando 2 Workers para Test 1A..."
            
            # Worker 1
            cat > worker1.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=1024
RETARDO_MEMORIA=500
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker1.config 1 &
            WORKER1_PID=$!
            print_info "Worker 1 iniciado (PID: $WORKER1_PID)"
            
            # Worker 2
            cat > worker2.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=1024
RETARDO_MEMORIA=500
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker2.config 2 &
            WORKER2_PID=$!
            print_info "Worker 2 iniciado (PID: $WORKER2_PID)"
            ;;
            
        2) # Test 1B - 2 Workers (misma config que 1A)
            config_and_run_worker 1
            ;;
            
        3) # Test 2A - 1 Worker CLOCK-M
            print_info "Iniciando Worker CLOCK-M para Test 2A..."
            cat > worker.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=48
RETARDO_MEMORIA=250
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker.config 1 &
            WORKER_PID=$!
            print_info "Worker CLOCK-M iniciado (PID: $WORKER_PID)"
            ;;
            
        4) # Test 2B - 1 Worker LRU
            print_info "Iniciando Worker LRU para Test 2B..."
            cat > worker.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=48
RETARDO_MEMORIA=250
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker.config 1 &
            WORKER_PID=$!
            print_info "Worker LRU iniciado (PID: $WORKER_PID)"
            ;;
            
        5) # Test 3 - 1 Worker
            print_info "Iniciando Worker para Test 3..."
            cat > worker.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=256
RETARDO_MEMORIA=250
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker.config 1 &
            WORKER_PID=$!
            print_info "Worker iniciado (PID: $WORKER_PID)"
            ;;
            
        6|7) # Tests 4A, 4B - 1 Worker
            print_info "Iniciando Worker para Test 4..."
            cat > worker.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=128
RETARDO_MEMORIA=250
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker.config 1 &
            WORKER_PID=$!
            print_info "Worker iniciado (PID: $WORKER_PID)"
            ;;
            
        8) # Test 5 - Múltiples Workers
            print_info "Iniciando 6 Workers para Test 5..."
            
            # Worker 1
            cat > worker1.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=128
RETARDO_MEMORIA=100
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker1.config 1 &
            print_info "Worker 1 iniciado (PID: $!)"
            
            # Worker 2
            cat > worker2.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=256
RETARDO_MEMORIA=50
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker2.config 2 &
            print_info "Worker 2 iniciado (PID: $!)"
            
            # Worker 3
            cat > worker3.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=64
RETARDO_MEMORIA=150
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker3.config 3 &
            print_info "Worker 3 iniciado (PID: $!)"
            
            # Worker 4
            cat > worker4.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=96
RETARDO_MEMORIA=125
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker4.config 4 &
            print_info "Worker 4 iniciado (PID: $!)"
            
            # Worker 5
            cat > worker5.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=160
RETARDO_MEMORIA=75
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker5.config 5 &
            print_info "Worker 5 iniciado (PID: $!)"
            
            # Worker 6
            cat > worker6.config << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=$MASTER_PORT
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=$STORAGE_PORT
TAM_MEMORIA=192
RETARDO_MEMORIA=175
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$BASE_DIR/queries
LOG_LEVEL=INFO
EOF
            ./bin/worker worker6.config 6 &
            print_info "Worker 6 iniciado (PID: $!)"
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
            print_info "Ejecutando queries FIFO..."
            ./bin/query_control query_control.config "$BASE_DIR/queries/FIFO_1" 4
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/FIFO_2" 3
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/FIFO_3" 5
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/FIFO_4" 1
            ;;
        2) # Test 1B - PRIORIDADES
            print_info "Ejecutando queries AGING..."
            ./bin/query_control query_control.config "$BASE_DIR/queries/AGING_1" 4
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/AGING_2" 3
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/AGING_3" 5
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/AGING_4" 1
            ;;
        3) # Test 2A - CLOCK-M
            print_info "Ejecutando MEMORIA_WORKER..."
            ./bin/query_control query_control.config "$BASE_DIR/queries/MEMORIA_WORKER" 0
            ;;
        4) # Test 2B - LRU
            print_info "Ejecutando MEMORIA_WORKER_2..."
            ./bin/query_control query_control.config "$BASE_DIR/queries/MEMORIA_WORKER_2" 0
            ;;
        5) # Test 3 - Errores
            print_info "Ejecutando queries de error..."
            ./bin/query_control query_control.config "$BASE_DIR/queries/ESCRITURA_ARCHIVO_COMMITED" 1
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/FILE_EXISTENTE" 1
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/LECTURA_FUERA_DEL_LIMITE" 1
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/TAG_EXISTENTE" 1
            ;;
        6) # Test 4A - Storage primera mitad
            print_info "Ejecutando STORAGE 1-4..."
            ./bin/query_control query_control.config "$BASE_DIR/queries/STORAGE_1" 0
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/STORAGE_2" 2
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/STORAGE_3" 4
            sleep 2
            ./bin/query_control query_control.config "$BASE_DIR/queries/STORAGE_4" 6
            ;;
        7) # Test 4B - Storage segunda mitad
            print_info "Ejecutando STORAGE_5..."
            ./bin/query_control query_control.config "$BASE_DIR/queries/STORAGE_5" 0
            ;;
        8) # Test 5 - Estabilidad
            print_info "Ejecutando 100 queries AGING en segundo plano..."
            for i in {1..25}; do
                ./bin/query_control query_control.config "$BASE_DIR/queries/AGING_1" 20 &
                ./bin/query_control query_control.config "$BASE_DIR/queries/AGING_2" 20 &
                ./bin/query_control query_control.config "$BASE_DIR/queries/AGING_3" 20 &
                ./bin/query_control query_control.config "$BASE_DIR/queries/AGING_4" 20 &
            done
            print_info "Todas las queries lanzadas. Ejecutándose en segundo plano..."
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

print_info "Configuración completada para Test $TEST_CHOICE"
print_info "IPs guardadas en: $IP_FILE"