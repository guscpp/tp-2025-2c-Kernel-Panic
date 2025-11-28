#!/bin/bash

# Script para ejecutar 25 queries con demora configurable y guardar logs
# Uso: ./ejecutar_queries.sh

echo "=== Script de Ejecución de 25 Queries ==="

# Solicitar el script a ejecutar
read -p "Ingrese el nombre del SCRIPT (ej: AGING_1, AGING_2, etc.): " script_name

# Solicitar la prioridad
read -p "Ingrese la PRIORIDAD (ej: 20): " prioridad

# Solicitar el tiempo de demora entre queries (en segundos)
read -p "Ingrese el tiempo de demora entre queries (en segundos): " demora

# Definir directorio de logs
LOG_DIR="logs_de_tests/test_5-25_queries"

echo ""
echo "Resumen de configuración:"
echo "SCRIPT: $script_name"
echo "PRIORIDAD: $prioridad"
echo "DEMORA: ${demora} segundos"
echo "CANTIDAD: 25 queries"
echo "DIRECTORIO LOGS: $LOG_DIR"
echo ""

# Crear directorio de logs si no existe
if [ ! -d "$LOG_DIR" ]; then
    echo "Creando directorio de logs: $LOG_DIR"
    mkdir -p "$LOG_DIR"
fi

read -p "¿Continuar con la ejecución? (y/n): " confirmar

if [ "$confirmar" != "y" ] && [ "$confirmar" != "Y" ]; then
    echo "Ejecución cancelada."
    exit 1
fi

echo ""
echo "Iniciando ejecución de 25 queries..."
echo ""

# Contador de queries ejecutadas
contador=1

# Ejecutar 25 queries
while [ $contador -le 25 ]
do
    # Nombre del archivo de log
    log_file="${LOG_DIR}/${script_name}_${contador}.log"
    
    echo "Ejecutando Query $contador/25 - SCRIPT: $script_name, PRIORIDAD: $prioridad"
    echo "Log: $log_file"
    
    # EJECUCIÓN REAL - Reemplaza con tu comando específico
    # Guarda tanto stdout como stderr en el archivo de log
    ./query_control SCRIPT=$script_name PRIORIDAD=$prioridad > "$log_file" 2>&1
    
    echo "✅ Log guardado en: $log_file"
    
    # Incrementar contador
    contador=$((contador + 1))
    
    # Esperar antes de la siguiente query (excepto después de la última)
    if [ $contador -le 25 ]; then
        echo "Esperando ${demora} segundos..."
        sleep $demora
        echo ""
    fi
done

echo ""
echo "✅ Ejecución completada - 25 queries enviadas"
echo "Script: $script_name"
echo "Prioridad: $prioridad"
echo "Logs guardados en: $LOG_DIR"
echo ""

# Mostrar resumen de archivos creados
echo "=== Resumen de logs creados ==="
ls -la "$LOG_DIR/${script_name}"_*.log 2>/dev/null || echo "No se encontraron logs para ${script_name}"