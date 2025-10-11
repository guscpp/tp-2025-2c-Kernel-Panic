#ifndef STORAGE_H
#define STORAGE_H

#include "../../utils/include/utils.h"
#include "colaSafe.h"
#include <commons/string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>



typedef struct {
    t_log* logger;
    t_config* config;
    char* puerto_escucha;
    int fresh_start;
    char* punto_montaje;
    int retardo_operacion;
    int retardo_acceso_bloque;
    char* log_level;   
} t_storage;

// t_storage* storage_general;

typedef struct{
    t_storage* storage;
    cola_tareas* cola_tareas;
} args_hilo_consumidor;

typedef struct{
    int socket_cliente;
    t_storage* storage;
    cola_tareas* cola;
} args_hilo_productor;


t_storage* iniciar_storage();
void verificar_storage(t_storage* s);
void liberar_storage (t_storage* storage);
void enviar_tamanio_paquete_aworker(int worker_fd, t_storage* storage);

void* rutina_recepcion(void* args);
void* rutina_operaciones(void* args);

#endif