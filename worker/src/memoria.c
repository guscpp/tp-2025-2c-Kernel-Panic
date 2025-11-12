// worker/src/memoria.c
#include "memoria.h"
#include <commons/string.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../../utils/include/utils.h"

bool error_memoria = false;

char* clave_file_tag(char* file, char* tag) {
    return string_from_format("%s:%s", file, tag);
}

t_entrada_pagina* buscar_pagina(t_memoria_interna* mem, char* file, char* tag, int num_pagina) {
    if (!mem || !file || !tag) return NULL;
    char* clave = clave_file_tag(file, tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    free(clave);
    if (!tabla) return NULL;

    bool es_pagina(void* elem) {
        t_entrada_pagina* e = (t_entrada_pagina*) elem;
        return e->numero_pagina == num_pagina;
    }
    t_entrada_pagina* encontrada = list_find(tabla, es_pagina);
    if (encontrada) {
        encontrada->ultimo_acceso = time(NULL);
        encontrada->bit_referencia = true;
        if (mem->algoritmo_reemplazo == LRU) {
            list_remove_element(mem->lru_list, encontrada);
            list_add(mem->lru_list, encontrada);
        }
    }
    return encontrada;
}

int encontrar_marco_libre(t_memoria_interna* mem) {
    for (int i = 0; i < mem->cantidad_marcos; i++) {
        if (mem->marcos[i]->libre) return i;
    }
    return -1;
}

// --- LRU ---
int aplicar_lru(t_memoria_interna* mem, int query_id) {
    if (list_is_empty(mem->lru_list)) return 0;
    t_entrada_pagina* victima = list_remove(mem->lru_list, 0);
    int marco_victima = victima->marco;

    log_info(mem->logger, "## Query<%d>: Se reemplaza la pagina %s:%s/%d por una nueva pagina",
             query_id, victima->file, victima->tag, victima->numero_pagina);
    if (victima->modificada) {
        log_info(mem->logger, "Query<%d>: Pagina modificada, deberia hacer FLUSH", query_id);
    }

    // Eliminar de tabla
    char* clave = clave_file_tag(victima->file, victima->tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    if (tabla) {
        bool misma_entrada(void* e) { return e == victima; }
        list_remove_by_condition(tabla, misma_entrada);
        if (list_is_empty(tabla)) {
            dictionary_remove_and_destroy(mem->tablas_paginas, clave, (void*)list_destroy);
        } else {
            free(clave);
        }
    } else {
        free(clave);
    }

    mem->marcos[marco_victima]->libre = true;
    mem->marcos[marco_victima]->entrada_pagina = NULL;
    free(victima->file);
    free(victima->tag);
    free(victima);
    return marco_victima;
}

// --- CLOCK-M ---
int aplicar_clock_m(t_memoria_interna* mem, int query_id) {
    t_clock_m* clock = mem->clock_m;
    int inicio = clock->puntero;
    int victima = -1;

    // 1. Marco libre
    for (int i = 0; i < clock->cantidad_marcos; i++) {
        int idx = (inicio + i) % clock->cantidad_marcos;
        if (mem->marcos[idx]->libre) {
            clock->puntero = (idx + 1) % clock->cantidad_marcos;
            return idx;
        }
    }

    // 2. Buscar (R=0, M=0)
    for (int i = 0; i < clock->cantidad_marcos; i++) {
        int idx = (inicio + i) % clock->cantidad_marcos;
        if (!clock->bits_referencia[idx] && !clock->bits_modificados[idx]) {
            victima = idx; break;
        }
    }

    // 3. Buscar (R=0, M=1)
    if (victima == -1) {
        for (int i = 0; i < clock->cantidad_marcos; i++) {
            int idx = (inicio + i) % clock->cantidad_marcos;
            if (!clock->bits_referencia[idx] && clock->bits_modificados[idx]) {
                victima = idx; break;
            }
        }
    }

    // 4. Limpiar R y buscar (R=0, M=X)
    if (victima == -1) {
        for (int i = 0; i < clock->cantidad_marcos; i++) clock->bits_referencia[i] = false;
        for (int i = 0; i < clock->cantidad_marcos; i++) {
            int idx = (inicio + i) % clock->cantidad_marcos;
            if (!clock->bits_referencia[idx]) {
                victima = idx; break;
            }
        }
    }

    if (victima == -1) victima = clock->puntero;
    clock->puntero = (victima + 1) % clock->cantidad_marcos;

    t_entrada_pagina* entrada_victima = mem->marcos[victima]->entrada_pagina;
    if (!entrada_victima) return victima;

    log_info(mem->logger, "## Query<%d>: Se reemplaza la pagina %s:%s/%d por una nueva pagina",
             query_id, entrada_victima->file, entrada_victima->tag, entrada_victima->numero_pagina);
    if (entrada_victima->modificada) {
        log_info(mem->logger, "Query<%d>: Pagina modificada, deberia hacer FLUSH", query_id);
    }

    // Eliminar de tabla
    char* clave = clave_file_tag(entrada_victima->file, entrada_victima->tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    if (tabla) {
        bool misma_entrada(void* e) { return e == entrada_victima; }
        list_remove_by_condition(tabla, misma_entrada);
        if (list_is_empty(tabla)) {
            dictionary_remove_and_destroy(mem->tablas_paginas, clave, (void*)list_destroy);
        } else {
            free(clave);
        }
    } else {
        free(clave);
    }

    mem->marcos[victima]->libre = true;
    mem->marcos[victima]->entrada_pagina = NULL;
    free(entrada_victima->file);
    free(entrada_victima->tag);
    free(entrada_victima);
    clock->bits_referencia[victima] = false;
    clock->bits_modificados[victima] = false;
    return victima;
}

int cargar_pagina(t_memoria_interna* mem, int query_id, char* file, char* tag, int num_pagina) {
    int marco = encontrar_marco_libre(mem);

    if (marco == -1) {
        if (mem->algoritmo_reemplazo == LRU)
            marco = aplicar_lru(mem, query_id);
        else
            marco = aplicar_clock_m(mem, query_id);
    }

    /*
    //agregar la logica de pedirle a storage el bloque
    //si se responde que hubo error: 
    //bloque = pedirBloqueAStorage()
    //if(!bloque){
            error_memoria = true; 
            avisar_error_generico(mem->logger, WORKER_ERROR_DIRECCION_INVALIDA); pedi un bloque logico que no es mio
            return -2
    }
    */
    mem->marcos[marco]->libre = false;
    t_entrada_pagina* nueva = malloc(sizeof(t_entrada_pagina));
    nueva->file = string_duplicate(file);
    nueva->tag = string_duplicate(tag);
    nueva->numero_pagina = num_pagina;
    nueva->marco = marco;
    nueva->modificada = false;
    nueva->ultimo_acceso = time(NULL);
    nueva->bit_referencia = true;
    mem->marcos[marco]->entrada_pagina = nueva;

    // Registrar en tabla
    char* clave = clave_file_tag(file, tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    if (!tabla) {
        tabla = list_create();
        dictionary_put(mem->tablas_paginas, clave, tabla);
    } else {
        free(clave);
    }
    list_add(tabla, nueva);

    if (mem->algoritmo_reemplazo == LRU) {
        list_add(mem->lru_list, nueva);
    }
    if (mem->algoritmo_reemplazo == CLOCK_M) {
        mem->clock_m->bits_referencia[marco] = true;
        mem->clock_m->bits_modificados[marco] = false;
    }

    // Logs obligatorios
    log_info(mem->logger, "Query<%d>: - Memoria Add - File:%s - Tag:%s - Pagina:%d - Marco:%d",
             query_id, file, tag, num_pagina, marco);
    log_info(mem->logger, "Query<%d>: Se asigna el Marco:%d a la Pagina:%d perteneciente al - File:%s - Tag:%s",
             query_id, marco, num_pagina, file, tag);

    // Inicializar con ceros
    void* dir = mem->memory_arena + marco * mem->tamanio_pagina;
    memset(dir, 0, mem->tamanio_pagina);
    return marco;
}

void* acceder_memoria(t_memoria_interna* mem, int query_id, char* file, char* tag, int offset, size_t tam, bool es_escritura) {
    if (!mem || !file || !tag || tam == 0) return NULL; //no se si haria falta mandar como error el motivo de que no quiera escribir nada
    int num_pagina = offset / mem->tamanio_pagina;
    int despl = offset % mem->tamanio_pagina;
    if (despl + tam > mem->tamanio_pagina) { //SI Inetna leer en algo que esta fuera del tamanio de pagina 
        log_error(mem->logger, "Query<%d>: Acceso cruza limite de pagina - Offset: %d, Tamaño: %zu", query_id, offset, tam);
        avisar_error_generico(mem->logger, WORKER_ERROR_SUPERA_TAMPAG);
        error_memoria = true;
        return NULL;
    }

    t_entrada_pagina* entrada = buscar_pagina(mem, file, tag, num_pagina);
    if (!entrada) {
        log_info(mem->logger, "Query<%d>: - Memoria Miss - File:%s - Tag:%s - Pagina:%d", //pf
                 query_id, file, tag, num_pagina);
        int marco = cargar_pagina(mem, query_id, file, tag, num_pagina);
        /*
        if (marco == -2){
            return NILL;
        }
        */
        entrada = buscar_pagina(mem, file, tag, num_pagina);
        if (!entrada) return NULL;
    }

    if (mem->algoritmo_reemplazo == CLOCK_M) {
        int m = entrada->marco;
        mem->clock_m->bits_referencia[m] = true;
        if (es_escritura) {
            mem->clock_m->bits_modificados[m] = true;
            entrada->modificada = true;
        }
    } else if (es_escritura) {
        entrada->modificada = true;
    }

    usleep(mem->retardo_memoria * 1000);
    void* dir_fisica = mem->memory_arena + entrada->marco * mem->tamanio_pagina + despl;

    // Logs obligatorios con valor
    char* valor_str = string_substring(dir_fisica, 0, tam);
    if (es_escritura) {
        log_info(mem->logger, "Query<%d>: Accion:ESCRIBIR - Direccion Fisica:%p - Valor:%s",
                 query_id, dir_fisica, valor_str);
    } else {
        log_info(mem->logger, "Query<%d>: Accion:LEER - Direccion Fisica:%p - Valor:%s",
                 query_id, dir_fisica, valor_str);
    }
    free(valor_str);
    return dir_fisica;
}

t_clock_m* crear_clock_m(int cantidad_marcos, t_marco** marcos) {
    t_clock_m* c = malloc(sizeof(t_clock_m));
    c->marcos = marcos;
    c->puntero = 0;
    c->cantidad_marcos = cantidad_marcos;
    c->bits_referencia = calloc(cantidad_marcos, sizeof(bool));
    c->bits_modificados = calloc(cantidad_marcos, sizeof(bool));
    return c;
}

void destruir_clock_m(t_clock_m* c) {
    if (!c) return;
    free(c->bits_referencia);
    free(c->bits_modificados);
    free(c);
}

t_memoria_interna* crear_memoria(t_log* logger, int tam_memoria, int retardo_memoria, char* algoritmo_str, int block_size) {
    t_memoria_interna* m = calloc(1, sizeof(t_memoria_interna));
    m->logger = logger;
    m->tamanio_memoria = tam_memoria;
    m->tamanio_pagina = block_size;
    m->cantidad_marcos = tam_memoria / block_size;
    m->retardo_memoria = retardo_memoria;
    m->algoritmo_reemplazo = (strcmp(algoritmo_str, "LRU") == 0) ? LRU : CLOCK_M;

    m->memory_arena = malloc(tam_memoria);
    memset(m->memory_arena, 0, tam_memoria);

    m->marcos = malloc(sizeof(t_marco*) * m->cantidad_marcos);
    for (int i = 0; i < m->cantidad_marcos; i++) {
        m->marcos[i] = malloc(sizeof(t_marco));
        m->marcos[i]->numero_marco = i;
        m->marcos[i]->libre = true;
        m->marcos[i]->entrada_pagina = NULL;
    }

    m->lru_list = (m->algoritmo_reemplazo == LRU) ? list_create() : NULL;
    m->clock_m = (m->algoritmo_reemplazo == CLOCK_M) ? crear_clock_m(m->cantidad_marcos, m->marcos) : NULL;
    m->tablas_paginas = dictionary_create();

    log_info(logger, "Memoria Interna creada: %d bytes, %d marcos, pagina=%d bytes, algoritmo=%s",
             m->tamanio_memoria, m->cantidad_marcos, m->tamanio_pagina, algoritmo_str);
    return m;
}

static void _destruir_tabla(char* key, void* value) {
    t_list* tabla = (t_list*) value;
    list_destroy(tabla);
}

void destruir_memoria(t_memoria_interna* m) {
    if (!m) return;
    free(m->memory_arena);
    for (int i = 0; i < m->cantidad_marcos; i++) free(m->marcos[i]);
    free(m->marcos);
    if (m->lru_list) list_destroy(m->lru_list);
    if (m->clock_m) destruir_clock_m(m->clock_m);
    dictionary_iterator(m->tablas_paginas, (void*)_destruir_tabla);
    dictionary_destroy(m->tablas_paginas);
    free(m);
}