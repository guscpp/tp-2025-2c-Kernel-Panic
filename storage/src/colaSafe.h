#ifndef COLA_SAFE_H
#define COLA_SAFE_H

#include <stdlib.h>
#include <pthread.h>
#include <commons/collections/list.h>
#include "../../utils/include/protocolo.h"


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

cola_tareas* crear_cola_tareas();
void encolar_tarea(cola_tareas* cola, Tarea* tarea);
void* desencolar_tarea(cola_tareas* cola);
void destruir_cola_tareas(cola_tareas* cola);



#endif