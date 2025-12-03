#!/bin/bash

# Script: ejecutar_tests_secuenciales.sh
# Ejecuta tests en orden y espera inactividad en storage.log + liberación de puertos

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_info()    { echo -e "${GREEN}[INFO]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error()   { echo -e "${RED}[ERROR]${NC} $1"; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(dirname "$SCRIPT_DIR")"

# === Variables globales configurables ===
STORAGE_LOG_TIMEOUT=30
INACTIVITY_THRESHOLD=30
# =======================================

# Función para matar procesos del TP
kill_tp_processes() {
    print_info "Finalizando procesos del TP..."
    pkill -f "./bin/query_control" 2>/dev/null || true
    pkill -f "./bin/worker" 2>/dev/null || true
    pkill -f "./bin/master" 2>/dev/null || true
    pkill -f "./bin/storage" 2>/dev/null || true
    sleep 2
}

# Función para liberar puertos
wait_for_ports_free() {
    print_info "Esperando liberación de puertos 9001 y 9002..."
    while true; do
        pid_9001=""
        pid_9002=""
        if command -v netstat &> /dev/null; then
            pid_9001=$(netstat -tnp 2>/dev/null | awk '$4 ~ /:9001$/ {split($7, a, "/"); print a[1]}' | head -n1)
            pid_9002=$(netstat -tnp 2>/dev/null | awk '$4 ~ /:9002$/ {split($7, a, "/"); print a[1]}' | head -n1)
        else
            print_error "netstat no disponible. Instálalo con: sudo apt install net-tools"
            exit 1
        fi

        if [[ -z "$pid_9001" && -z "$pid_9002" ]]; then
            print_info "Puertos 9001 y 9002 liberados."
            break
        else
            current_time=$(date '+%Y-%m-%d %H:%M:%S')
            echo "[$current_time] Puertos ocupados → 9001: $pid_9001, 9002: $pid_9002"
            sleep 2
        fi
    done
}

# Función para esperar inactividad en storage.log
wait_for_storage_inactivity() {
    local storage_log="$1"
    local test_name="$2"
    local timeout_wait="$3"
    local inactivity_threshold="$4"
    local last_size
    local current_size
    local last_change=$(date +%s)

    if [[ ! -f "$storage_log" ]]; then
        print_warning "Archivo storage.log no encontrado: $storage_log. Esperando creación (máx. ${timeout_wait}s)..."
        timeout "$timeout_wait" bash -c "while [[ ! -f '$storage_log' ]]; do sleep 1; done" || true
    fi

    if [[ ! -f "$storage_log" ]]; then
        print_warning "[$test_name] storage.log aún no existe tras ${timeout_wait}s. Saltando monitoreo."
        return
    fi

    last_size=$(stat -c%s "$storage_log")
    print_info "[$test_name] Monitoreando inactividad en: $storage_log (esperando ${inactivity_threshold}s sin cambios)"

    while true; do
        sleep 5
        current_size=$(stat -c%s "$storage_log" 2>/dev/null || echo "$last_size")
        if [[ "$current_size" != "$last_size" ]]; then
            last_size="$current_size"
            last_change=$(date +%s)
            print_info "[$test_name] Actividad detectada en storage.log (tamaño: $current_size)"
        fi

        now=$(date +%s)
        elapsed=$((now - last_change))
        if [[ $elapsed -ge $inactivity_threshold ]]; then
            print_info "[$test_name] Inactividad confirmada en storage.log por ${inactivity_threshold} segundos."
            break
        fi

        current_time_str=$(date '+%Y-%m-%d %H:%M:%S')
        print_info "[$test_name] [$current_time_str] Esperando... Última escritura hace ${elapsed}s de ${inactivity_threshold}s"
    done
}

# Función para copiar archivos de estado del storage
copy_storage_state() {
    local test_log_dir="$1"
    local test_name="$2"
    
    print_info "Copiando archivos de estado del storage para $test_name..."
    
    sleep 2
    
    if [[ -f "$BASE_DIR/storage/bitmap.bin" ]]; then
        cp "$BASE_DIR/storage/bitmap.bin" "$test_log_dir/"
        print_info "bitmap.bin copiado"
    else
        print_warning "bitmap.bin no encontrado"
    fi
    
    if [[ -f "$BASE_DIR/storage/blocks_hash_index.config" ]]; then
        cp "$BASE_DIR/storage/blocks_hash_index.config" "$test_log_dir/"
        print_info "blocks_hash_index.config copiado"
    else
        print_warning "blocks_hash_index.config no encontrado"
    fi
}

# Manejador de Ctrl+C
cleanup_and_exit() {
    print_error "Interrupción recibida (Ctrl+C). Finalizando..."
    kill_tp_processes
    wait_for_ports_free
    exit 1
}

# Registrar el trap
trap cleanup_and_exit INT TERM

# === Configuración inicial interactiva ===
read -r -p "¿Borrar logs existentes? (Enter = sí, n = no): " delete_logs
delete_logs="${delete_logs:-y}"
if [[ "$delete_logs" =~ ^[Yy]$|^$ ]]; then
    if [[ -d "./logs" ]]; then
        print_info "Borrando directorio ./logs..."
        rm -rf "./logs"
    else
        print_info "No existe ./logs. Nada que borrar."
    fi
fi

read -r -p "Timeout para esperar storage.log (segundos, Enter = 30): " timeout_input
timeout_input="${timeout_input:-30}"
if ! [[ "$timeout_input" =~ ^[0-9]+$ ]]; then
    print_error "Valor inválido. Usando 30 por defecto."
    timeout_input=30
fi
STORAGE_LOG_TIMEOUT="$timeout_input"

read -r -p "Umbral de inactividad en storage.log (segundos, Enter = 30): " inact_input
inact_input="${inact_input:-30}"
if ! [[ "$inact_input" =~ ^[0-9]+$ ]]; then
    print_error "Valor inválido. Usando 30 por defecto."
    inact_input=30
fi
INACTIVITY_THRESHOLD="$inact_input"
# =========================================

# Lista ordenada de tests
tests=(
    "test_1a-prueba_planificacion-fifo"
    "test_1b-prueba_planificacion-prioridades"
    "test_2a-prueba_memoria_worker-clock-m"
    "test_2b-prueba_memoria_worker-lru"
    "test_3-prueba_errores"
    "test_4a-prueba_storage"
    "test_4b-prueba_storage"
    "test_5-prueba_estabilidad_general"
)

# Validar que todos los scripts existan
for test in "${tests[@]}"; do
    if [[ ! -f "$test" ]]; then
        print_error "Script no encontrado: $test"
        exit 1
    fi
done

# Ejecutar cada test en orden
for test in "${tests[@]}"; do
    test_name="${test%.*}"
    print_info "Ejecutando $test_name"

    # Limpiar antes de cada test
    kill_tp_processes
    wait_for_ports_free

    test_log_dir="./logs/$test_name"
    mkdir -p "$test_log_dir"

    # Ejecutar test en background
    ./"$test" &
    test_pid=$!

    storage_log_path="$test_log_dir/storage.log"

    # Esperar inactividad con los valores configurados
    wait_for_storage_inactivity "$storage_log_path" "$test_name" "$STORAGE_LOG_TIMEOUT" "$INACTIVITY_THRESHOLD"

    # Copiar estado del storage
    copy_storage_state "$test_log_dir" "$test_name"

    # Limpiar al finalizar el test
    kill_tp_processes
    wait_for_ports_free

    print_info "Finalizado $test_name. Preparando siguiente prueba...\n"
done

print_info "✅ Todos los tests ejecutados exitosamente en secuencia."