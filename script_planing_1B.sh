#!/bin/bash

BASE_DIR="$(pwd)"
QUERIES_DIR="$BASE_DIR/queries"
QC="$BASE_DIR/query_control/bin/query_control"
CONFIG="$BASE_DIR/query_control/query_control.config"

print_info() { echo -e "\033[0;32m[INFO]\033[0m $1"; }
print_error() { echo -e "\033[0;31m[ERROR]\033[0m $1"; }

# Validar binarios
if [ ! -f "$QC" ]; then
    print_error "No se encontró query_control en: $QC"
    exit 1
fi

if [ ! -f "$CONFIG" ]; then
    print_error "No se encontró query_control.config"
    exit 1
fi

cd "$BASE_DIR/query_control" || exit 1

# === Ejecutar todas las queries en paralelo ===

print_info "Lanzando QUERY 1 en paralelo..."
($QC "$CONFIG" "$QUERIES_DIR/FIFO_1" 4 &)

print_info "Lanzando QUERY 2 en paralelo..."
($QC "$CONFIG" "$QUERIES_DIR/FIFO_2" 3 &)

print_info "Lanzando QUERY 3 en paralelo..."
($QC "$CONFIG" "$QUERIES_DIR/FIFO_3" 5 &)

print_info "Lanzando QUERY 4 en paralelo..."
($QC "$CONFIG" "$QUERIES_DIR/FIFO_4" 1 &)

print_info "Todas las queries fueron enviadas en paralelo."
wait
print_info "Todas las queries finalizaron."
