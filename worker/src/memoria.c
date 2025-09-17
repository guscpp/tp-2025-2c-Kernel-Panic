#include "memoria.h"
#include "../../utils/include/sueltos.h"

t_memoria_interna* crear_memoria(t_log* logger)
{
    t_memoria_interna* m = malloc(sizeof(t_memoria_interna));

    m->logger = logger;
    m->config = iniciar_config(m->logger, "worker.config");
    m->tamanio_memoria = config_get_int_value(m->config, "TAM_MEMORIA");
    m->retardo_memoria = config_get_int_value(m->config, "RETARDO_MEMORIA");
    m->tamanio_marco = 1024; // viene dado por storage como tamanio de pagina
    m->cantidad_marcos = m->tamanio_memoria / m->tamanio_marco;

    char* algoritmo = config_get_string_value(m->config, "ALGORITMO_REEMPLAZO");
    if (strcmp(algoritmo, "LRU") == 0) {
        m->algoritmo_reemplazo = LRU;
    } else if (strcmp(algoritmo, "CLOCK_M") == 0) {
        m->algoritmo_reemplazo = CLOCK_M;
    } else {
        // Default to LRU if unknown
        log_warning(m->logger, "Algoritmo desconocido: %s, usando LRU por defecto", algoritmo);
        m->algoritmo_reemplazo = LRU;
    }

    m->memory_arena = malloc(m->tamanio_memoria);
    // if (m->memory_arena) {
    //     memset(m->memory_arena, 0, m->tamanio_memoria); // inicializa todo en 0s
    //     log_info(logger, "Memoria allocada: %zu bytes (página: %zu bytes)",
    //         m->tamanio_memoria, m->tamanio_marco);
    // }

    return m;
}

void destruir_memoria(t_memoria_interna* memoria) {
    if (memoria) {
        if (memoria->memory_arena) free(memoria->memory_arena);
        free(memoria);
    }
}

//int asignar_marco(t_memoria_interna* memoria, char* file, char* tag, int numero_pagina) {}
//void liberar_marco(t_memoria_interna* memoria, int numero_marco) {}
//int obtener_marco(t_memoria_interna* memoria, char* file, char* tag, int numero_pagina) {}