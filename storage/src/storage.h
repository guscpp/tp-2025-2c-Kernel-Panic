#ifndef STORAGE_H
#define STORAGE_H

#include "../../utils/include/utils.h"
#include "../../utils/include/sueltos.h"
#include "../../utils/include/protocolo.h"
#include "../../utils/include/paquete.h"
#include "../../utils/include/conexion.h"

typedef struct {
    t_log* logger;
    t_config* config;
    char* puerto;
    int fresh;
    char* punto_montaje;
    int retardo_operacion;
    int retardo_acceso_bloque;
    char* log_level;   
} t_storage;

t_storage* iniciar_storage();
void liberar_storage (t_storage* storage);
// int inicializar_file_system(t_storage* storage);
// int formatear_fs(t_storage* storage);

#endif