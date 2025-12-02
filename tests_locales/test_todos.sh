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

# Función para matar procesos del TP
kill_tp_processes() {
    print_info "Finalizando procesos del TP..."
    pkill -f "./bin/query_control" 2>/dev/null || true
    pkill -f "./bin/worker" 2>/dev/null || true
    pkill -f "./bin/master" 2>/dev/null || true
    pkill -f "./bin/storage" 2>/dev/null || true
    sleep 3
}

# Función para verificar y esperar liberación de puertos 9001 y 9002
wait_for_ports_free() {
    print_info "Esperando liberación de puertos 9001 y 9002..."
    while true; do
        # Detectar PID usando netstat (compatible con enunciado)
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
            print_info "✅ Puertos 9001 y 9002 liberados."
            break
        else
            current_time=$(date '+%Y-%m-%d %H:%M:%S')
            echo "[$current_time] Puertos ocupados → 9001: $pid_9001, 9002: $pid_9002"
            sleep 2
        fi
    done
}

# Función para esperar inactividad en storage.log por 60 segundos
wait_for_storage_inactivity() {
    local storage_log="$1"
    local test_name="$2"  # <-- nuevo parámetro
    local inactivity_threshold=60
    local last_size
    local current_size
    local last_change=$(date +%s)

    if [[ ! -f "$storage_log" ]]; then
        print_warning "Archivo storage.log no encontrado: $storage_log. Esperando creación..."
        timeout 30 bash -c "while [[ ! -f '$storage_log' ]]; do sleep 1; done" || true
    fi

    if [[ ! -f "$storage_log" ]]; then
        print_warning "[$test_name] storage.log aún no existe. Saltando monitoreo."
        return
    fi

    last_size=$(stat -c%s "$storage_log")
    print_info "[$test_name] Monitoreando inactividad en: $storage_log (esperando $inactivity_threshold segundos sin cambios)"

    while true; do
        sleep 5
        current_size=$(stat -c%s "$storage_log" 2>/dev/null || echo "$last_size")
        if [[ "$current_size" != "$last_size" ]]; then
            last_size="$current_size"
            last_change=$(date +%s)
            print_info "[$test_name] 📝 Actividad detectada en storage.log (tamaño: $current_size)"
        fi

        now=$(date +%s)
        elapsed=$((now - last_change))
        if [[ $elapsed -ge $inactivity_threshold ]]; then
            print_info "[$test_name] ✅ Inactividad confirmada en storage.log por $inactivity_threshold segundos."
            break
        fi

        current_time_str=$(date '+%Y-%m-%d %H:%M:%S')
        print_info "[$test_name] [$current_time_str] Esperando... Última escritura hace ${elapsed}s"
    done
}

# Lista ordenada de tests
tests=(
    "test_1a-prueba_planificacion-fifo.sh"
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
    print_info "🚀 Ejecutando $test_name"

    # Limpiar procesos residuales antes de comenzar
    kill_tp_processes
    wait_for_ports_free

    # Ejecutar test en background
    ./"$test" &
    test_pid=$!

    # Determinar ruta del storage.log esperado
    # Suponemos que cada test crea logs en ./logs/<test_name>/storage.log
    storage_log_path="./logs/$test_name/storage.log"

    # Esperar inactividad en el storage.log correspondiente
    wait_for_storage_inactivity "$storage_log_path" "$test_name"

    # Finalizar el test (en caso de que no haya terminado solo)
    kill_tp_processes

    # Esperar que los puertos se liberen antes del siguiente test
    wait_for_ports_free

    print_info "✅ Finalizado $test_name. Preparando siguiente prueba...\n"
done

print_info "🎉 Todos los tests ejecutados exitosamente en secuencia."