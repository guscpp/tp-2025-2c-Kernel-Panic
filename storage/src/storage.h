#ifndef STORAGE_H
#define STORAGE_H

#include "../../utils/include/utils.h"
#include "../../utils/include/sueltos.h"
#include "../../utils/include/protocolo.h"
#include "../../utils/include/paquete.h"
#include "../../utils/include/conexion.h"
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

t_storage* storage_general;

typedef struct {
    t_list* lista_tareas;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} cola_tareas;


typedef struct{
    op_code codigo_operacion;
    void* parametros;
    int socket_cliente;
}Tarea;

t_storage* iniciar_storage();
void verificar_storage(t_storage* s);
void liberar_storage (t_storage* storage);
void enviar_tamanio_paquete_aworker(int worker_fd, t_storage* storage);
int  conseguir_tamanio_paquete();
void rutina_recepcion(void* args);
void rutina_operaciones();
extern cola_tareas* cola_tareas_global;

#endif