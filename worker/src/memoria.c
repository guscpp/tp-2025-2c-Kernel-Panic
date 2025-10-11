#include "../include/memoria.h"
#include <commons/string.h>

// Helper: clave única "file:tag"
static char* _clave_file_tag(char* file, char* tag) {
    return string_from_format("%s:%s", file, tag);
}

// Busca una página en la tabla correspondiente
static entrada_pagina_t* _buscar_pagina(t_memoria_interna* mem, char* file, char* tag, int num_pagina) {
    char* clave = _clave_file_tag(file, tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    free(clave);
    if (!tabla) return NULL;

    bool _es_pagina(void* elem) {
        entrada_pagina_t* e = (entrada_pagina_t*) elem;
        return e->numero_pagina == num_pagina;
    }
    return list_find(tabla, _es_pagina);
}

// Simula cargar una página desde Storage (mock en CP2)
static int _cargar_pagina_mock(t_memoria_interna* mem, int query_id, char* file, char* tag, int num_pagina) {
    // FIFO simple (puntero circular)
    static int puntero_fifo = 0;
    int marco = puntero_fifo;
    puntero_fifo = (puntero_fifo + 1) % mem->cantidad_marcos;

    // Si el marco estaba ocupado y modificado → FLUSH (solo log en CP2)
    if (mem->marco_ocupado[marco]) {
        // En CP3: aquí iría el flush real al Storage
        log_info(mem->logger, "Query<%d>: Se reemplaza la página de otro File:Tag en Marco:%d", query_id, marco);
    }

    // Simular lectura del Storage: llenar con 'X'
    void* dir_fisica = mem->memory_arena + marco * mem->tamanio_pagina;
    memset(dir_fisica, 'X', mem->tamanio_pagina);

    // Registrar en tabla de páginas
    char* clave = _clave_file_tag(file, tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    if (!tabla) {
        tabla = list_create();
        dictionary_put(mem->tablas_paginas, clave, tabla);
    } else {
        free(clave);
    }

    entrada_pagina_t* entrada = malloc(sizeof(entrada_pagina_t));
    entrada->file = string_duplicate(file);
    entrada->tag = string_duplicate(tag);
    entrada->numero_pagina = num_pagina;
    entrada->marco = marco;
    entrada->modificada = false;
    list_add(tabla, entrada);

    mem->marco_ocupado[marco] = true;

    // Logs obligatorios
    log_info(mem->logger, "Query<%d>: Memoria Add - File:%s - Tag:%s - Pagina:%d - Marco:%d",
             query_id, file, tag, num_pagina, marco);

    return marco;
}

// Acceso principal: READ o WRITE
void* acceder_memoria(t_memoria_interna* mem, int query_id, char* file, char* tag, int offset, size_t tam, bool es_escritura) {
    int num_pagina = offset / mem->tamanio_pagina;
    int despl = offset % mem->tamanio_pagina;

    entrada_pagina_t* entrada = _buscar_pagina(mem, file, tag, num_pagina);
    if (!entrada) {
        // MISS
        log_info(mem->logger, "Query<%d>: Memoria Miss - File:%s - Tag:%s - Pagina:%d",
                 query_id, file, tag, num_pagina);

        _cargar_pagina_mock(mem, query_id, file, tag, num_pagina);
        entrada = _buscar_pagina(mem, file, tag, num_pagina); // ahora sí existe
    }

    // Simular retardo de memoria
    usleep(mem->retardo_memoria * 1000);

    void* dir_fisica = mem->memory_arena + entrada->marco * mem->tamanio_pagina + despl;

    if (es_escritura) {
        entrada->modificada = true;
        log_info(mem->logger, "Query<%d>: Acción:ESCRIBIR - Dirección Física:%p", query_id, dir_fisica);
    } else {
        log_info(mem->logger, "Query<%d>: Acción:LEER - Dirección Física:%p", query_id, dir_fisica);
    }

    return dir_fisica;
}

// Inicialización
t_memoria_interna* crear_memoria(t_log* logger, int tam_memoria, int retardo_memoria, char* algoritmo_str, int block_size) {
    t_memoria_interna* m = malloc(sizeof(t_memoria_interna));
    m->logger = logger;
    m->tamanio_memoria = tam_memoria;
    m->tamanio_pagina = block_size;
    m->cantidad_marcos = m->tamanio_memoria / m->tamanio_pagina;
    m->retardo_memoria = retardo_memoria;

    if (strcmp(algoritmo_str, "LRU") == 0) {
        m->algoritmo_reemplazo = LRU;
    } else {
        m->algoritmo_reemplazo = CLOCK_M;
    }

    m->memory_arena = malloc(m->tamanio_memoria);
    memset(m->memory_arena, 0, m->tamanio_memoria);

    m->marco_ocupado = calloc(m->cantidad_marcos, sizeof(bool));
    m->tablas_paginas = dictionary_create();

    log_info(logger, "Memoria Interna creada: %d bytes, %d marcos, página=%d bytes",
             m->tamanio_memoria, m->cantidad_marcos, m->tamanio_pagina);
    return m;
}

// Función única que libera clave Y valor
static void _destruir_entrada_tabla(char* key, void* value) {
    // Liberar la clave (el string "File:Tag")
    free(key);
    // Destruir el valor (la lista de entradas de pagina)
    t_list* tabla = (t_list*) value;
    for (int i = 0; i < list_size(tabla); i++) {
        entrada_pagina_t* e = list_get(tabla, i);
        free(e->file);
        free(e->tag);
        free(e);
    }
    list_destroy(tabla);
}




void destruir_memoria(t_memoria_interna* memoria) {
    if (!memoria) return;

    // Iterar con UNA SOLA función
    dictionary_iterator(memoria->tablas_paginas, _destruir_entrada_tabla);

    dictionary_destroy(memoria->tablas_paginas);
    free(memoria->memory_arena);
    free(memoria->marco_ocupado);
    free(memoria);
}