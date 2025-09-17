#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <commons/log.h>
#include <commons/config.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef enum {
    LRU,
    CLOCK_M
} t_algoritmo_reemplazo;

typedef struct {
    int tamanio_memoria;
    int tamanio_pagina;
    int tamanio_marco;
    int cantidad_marcos;
    t_algoritmo_reemplazo algoritmo_reemplazo;
    int retardo_memoria;
    t_log* logger;
    t_config* config;
    void* memory_arena;
} t_memoria_interna;

t_memoria_interna* crear_memoria(t_log* logger);
void destruir_memoria(t_memoria_interna* memoria);
//int asignar_marco(t_memoria_interna* memoria, char* file, char* tag, int numero_pagina);
//void liberar_marco(t_memoria_interna* memoria, int numero_marco);
//int obtener_marco(t_memoria_interna* memoria, char* file, char* tag, int numero_pagina);

#endif