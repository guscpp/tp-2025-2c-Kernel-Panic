#include "colaSafe.h"
cola_tareas* crear_cola_tareas(){
    cola_tareas* cola = malloc(sizeof(cola_tareas));
    cola->lista_tareas = list_create();
    pthread_mutex_init(&cola->mutex, NULL);
    pthread_cond_init(&cola->cond, NULL);
    return cola;
}

void encolar_tarea(cola_tareas* cola, Tarea* tarea){
    pthread_mutex_lock(&cola->mutex);
    list_add(cola->lista_tareas, tarea);
    pthread_cond_signal(&cola->cond);
    pthread_mutex_unlock(&cola->mutex);
}

void* desencolar_tarea(cola_tareas* cola){
    pthread_mutex_lock(&cola->mutex);
    while(list_is_empty(cola->lista_tareas)){
        pthread_cond_wait(&cola->cond, &cola->mutex); // si la lista esta vacia, espera
    }
    void* tarea = list_remove(cola->lista_tareas, 0);
    pthread_mutex_unlock(&cola->mutex);

    return tarea;
}

void destruir_cola_tareas(cola_tareas* cola){
    list_destroy_and_destroy_elements(cola->lista_tareas, free);
    pthread_mutex_destroy(&cola->mutex);
    pthread_cond_destroy(&cola->cond);
    free(cola);
}