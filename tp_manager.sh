#!/bin/bash
# tp_manager.sh
# Configura .config según test seleccionado, IPs y nivel de log
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Detectar la raíz del proyecto (donde están las carpetas master, storage, etc.)
PROJECT_ROOT="$(pwd)"
if [ ! -d "$PROJECT_ROOT/master" ] || [ ! -d "$PROJECT_ROOT/storage" ] || [ ! -d "$PROJECT_ROOT/worker" ]; then
    print_error "Este script debe ejecutarse desde la raíz del proyecto."
    print_error "Asegúrate de estar en el directorio que contiene las carpetas master/, storage/, worker/, etc."
    exit 1
fi

IP_FILE="$PROJECT_ROOT/dirs_ip"

# === Cargar IPs existentes ===
MASTER_IP=""
STORAGE_IP=""

if [ -f "$IP_FILE" ]; then
    source "$IP_FILE" 2>/dev/null
fi

# === Preguntar por IPs ===
if [ -n "$MASTER_IP" ] && [ -n "$STORAGE_IP" ]; then
    print_info "IPs actuales detectadas:"
    echo "  Master: $MASTER_IP"
    echo "  Storage: $STORAGE_IP"
    read -p "¿Desea usar estas IPs? (s/n, Enter = sí): " -r resp
    # Si resp está vacío o es 's'/'S', se mantienen las IPs
    if [[ -z "$resp" || "$resp" =~ ^[Ss]$ ]]; then
        :  # Aceptar (no hacer nada)
    else
        MASTER_IP=""
        STORAGE_IP=""
    fi
fi

if [ -z "$MASTER_IP" ]; then
    read -p "Ingrese IP del Master: " -r MASTER_IP
    read -p "Ingrese IP del Storage: " -r STORAGE_IP
    echo "MASTER_IP=\"$MASTER_IP\"" > "$IP_FILE"
    echo "STORAGE_IP=\"$STORAGE_IP\"" >> "$IP_FILE"
    chmod 600 "$IP_FILE"
    print_info "IPs guardadas en $IP_FILE"
fi

# === Preguntar por LOG_LEVEL con menú numérico ===
print_info "Seleccione el nivel de LOG:"
echo "1) INFO"
echo "2) DEBUG"
read -p "Opción (1-2, Enter = 1): " -r log_choice

case "$log_choice" in
    2) LOG_LEVEL="DEBUG" ;;
    ""|1) LOG_LEVEL="INFO" ;;
    *)
        print_warning "Opción inválida. Usando INFO por defecto."
        LOG_LEVEL="INFO"
        ;;
esac
print_info "LOG_LEVEL configurado como: $LOG_LEVEL"

# === Función para aplicar config de test ===
apply_test_config() {
    local test_name="$1"
    local QUERIES_DIR="$PROJECT_ROOT/queries"

    case "$test_name" in
        "Test 1A - Prueba Planificacion FIFO")
            cat > "$PROJECT_ROOT/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=500
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=TRUE
PUNTO_MONTAJE=$PROJECT_ROOT/storage
RETARDO_OPERACION=25
RETARDO_ACCESO_BLOQUE=25
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
            cat > "$PROJECT_ROOT/worker/worker1.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=1024
RETARDO_MEMORIA=50
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/worker/worker2.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=1024
RETARDO_MEMORIA=50
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/query_control/query_control.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
LOG_LEVEL=$LOG_LEVEL
EOF
            ;;

        "Test 1B - Prueba Planificacion PRIORIDADES")
            cat > "$PROJECT_ROOT/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=PRIORIDADES
TIEMPO_AGING=500
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=FALSE
PUNTO_MONTAJE=$PROJECT_ROOT/storage
RETARDO_OPERACION=25
RETARDO_ACCESO_BLOQUE=25
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
            cat > "$PROJECT_ROOT/worker/worker1.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=1024
RETARDO_MEMORIA=50
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/worker/worker2.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=1024
RETARDO_MEMORIA=50
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/query_control/query_control.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
LOG_LEVEL=$LOG_LEVEL
EOF
            ;;

        "Test 2A - Prueba Memoria Worker CLOCK-M")
            cat > "$PROJECT_ROOT/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=0
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=FALSE
PUNTO_MONTAJE=$PROJECT_ROOT/storage
RETARDO_OPERACION=50
RETARDO_ACCESO_BLOQUE=50
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
            cat > "$PROJECT_ROOT/worker/worker.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=48
RETARDO_MEMORIA=25
ALGORITMO_REEMPLAZO=CLOCK-M
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/query_control/query_control.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
LOG_LEVEL=$LOG_LEVEL
EOF
            ;;

        "Test 2B - Prueba Memoria Worker LRU")
            cat > "$PROJECT_ROOT/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=0
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=FALSE
PUNTO_MONTAJE=$PROJECT_ROOT/storage
RETARDO_OPERACION=50
RETARDO_ACCESO_BLOQUE=50
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
            cat > "$PROJECT_ROOT/worker/worker.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=48
RETARDO_MEMORIA=25
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/query_control/query_control.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
LOG_LEVEL=$LOG_LEVEL
EOF
            ;;

        "Test 3  - Prueba Errores")
            cat > "$PROJECT_ROOT/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=FIFO
TIEMPO_AGING=0
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=FALSE
PUNTO_MONTAJE=$PROJECT_ROOT/storage
RETARDO_OPERACION=50
RETARDO_ACCESO_BLOQUE=50
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
            cat > "$PROJECT_ROOT/worker/worker.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=256
RETARDO_MEMORIA=25
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/query_control/query_control.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
LOG_LEVEL=$LOG_LEVEL
EOF
            ;;

        "Test 4A - Prueba Storage")
            cat > "$PROJECT_ROOT/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=PRIORIDADES
TIEMPO_AGING=0
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=FALSE
PUNTO_MONTAJE=$PROJECT_ROOT/storage
RETARDO_OPERACION=50
RETARDO_ACCESO_BLOQUE=50
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
            cat > "$PROJECT_ROOT/worker/worker.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=128
RETARDO_MEMORIA=25
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/query_control/query_control.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
LOG_LEVEL=$LOG_LEVEL
EOF
            ;;

        "Test 4B - Prueba Storage")
            # Configuración idéntica a 4A según el PDF
            cat > "$PROJECT_ROOT/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=PRIORIDADES
TIEMPO_AGING=0
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=FALSE
PUNTO_MONTAJE=$PROJECT_ROOT/storage
RETARDO_OPERACION=50
RETARDO_ACCESO_BLOQUE=50
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
            cat > "$PROJECT_ROOT/worker/worker.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=128
RETARDO_MEMORIA=25
ALGORITMO_REEMPLAZO=LRU
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/query_control/query_control.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
LOG_LEVEL=$LOG_LEVEL
EOF
            ;;

        "Test 5  - Prueba Estabilidad General")
            cat > "$PROJECT_ROOT/master/master.config" << EOF
PUERTO_ESCUCHA=9001
ALGORITMO_PLANIFICACION=PRIORIDADES
TIEMPO_AGING=25
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/storage.config" << EOF
PUERTO_ESCUCHA=9002
FRESH_START=FALSE
PUNTO_MONTAJE=$PROJECT_ROOT/storage
RETARDO_OPERACION=100
RETARDO_ACCESO_BLOQUE=10
LOG_LEVEL=$LOG_LEVEL
EOF
            cat > "$PROJECT_ROOT/storage/superblock.config" << EOF
BLOCK_SIZE=16
FS_SIZE=65536
EOF
            local wconf=(
                "128 10 LRU"
                "256 5 CLOCK-M"
                "64 15 CLOCK-M"
                "96 15 LRU"
                "160 10 LRU"
                "192 15 CLOCK-M"
            )
            for i in {0..5}; do
                read tam ret alg <<< "${wconf[$i]}"
                cat > "$PROJECT_ROOT/worker/worker$((i+1)).config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
IP_STORAGE=$STORAGE_IP
PUERTO_STORAGE=9002
TAM_MEMORIA=$tam
RETARDO_MEMORIA=$ret
ALGORITMO_REEMPLAZO=$alg
PATH_QUERIES=$QUERIES_DIR
LOG_LEVEL=$LOG_LEVEL
EOF
            done
            cat > "$PROJECT_ROOT/query_control/query_control.config" << EOF
IP_MASTER=$MASTER_IP
PUERTO_MASTER=9001
LOG_LEVEL=$LOG_LEVEL
EOF
            ;;

        *)
            print_error "Test no reconocido: $test_name"
            exit 1
            ;;
    esac
}

# === Menú de selección de test ===
print_info "Seleccione el test a configurar:"
tests=(
    "Test 1A - Prueba Planificacion FIFO"
    "Test 1B - Prueba Planificacion PRIORIDADES"
    "Test 2A - Prueba Memoria Worker CLOCK-M"
    "Test 2B - Prueba Memoria Worker LRU"
    "Test 3  - Prueba Errores"
    "Test 4A - Prueba Storage"
    "Test 4B - Prueba Storage"
    "Test 5  - Prueba Estabilidad General"
)
select test_name in "${tests[@]}"; do
    if [[ -n "$test_name" ]]; then
        break
    else
        print_error "Opción inválida. Seleccione un número del 1 al 8."
    fi
done

# === Aplicar configuración ===
apply_test_config "$test_name"
print_info "✅ Configuración aplicada para: $test_name"
print_info "   Master IP: $MASTER_IP"
print_info "   Storage IP: $STORAGE_IP"
print_info "   LOG_LEVEL: $LOG_LEVEL"