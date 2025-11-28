#!/bin/bash
# Ejecuta las queries y muestra directamente en pantalla lo que logea cada una

print_info() { echo -e "[INFO] $1"; }

BASE_DIR="$(pwd)"
QUERIES_DIR="$BASE_DIR/queries"

cd "$BASE_DIR/query_control"

print_info "Lanzando queries AGING y mostrando sus logs directamente en pantalla"

echo
echo "===== QUERY 1 (AGING_1, prioridad 4) ====="
./bin/query_control query_control.config "$QUERIES_DIR/AGING_1" 4 &
PID1=$!

sleep 2

echo
echo "===== QUERY 2 (AGING_2, prioridad 3) ====="
./bin/query_control query_control.config "$QUERIES_DIR/AGING_2" 3 &
PID2=$!

sleep 2

echo
echo "===== QUERY 3 (AGING_3, prioridad 5) ====="
./bin/query_control query_control.config "$QUERIES_DIR/AGING_3" 5 &
PID3=$!

sleep 2

echo
echo "===== QUERY 4 (AGING_4, prioridad 1) ====="
./bin/query_control query_control.config "$QUERIES_DIR/AGING_4" 1 &
PID4=$!

echo
print_info "Queries enviadas. Esperando que todas terminen..."
wait $PID1 $PID2 $PID3 $PID4

echo
print_info "=== Todas las queries finalizaron ==="