#include "../include/memoria.h"
#include <commons/string.h>

// Helper: clave unica "file:tag"
char* clave_file_tag(char* file, char* tag) {
    return string_from_format("%s:%s", file, tag);
}

// Busca una pagina en la tabla correspondiente
t_entrada_pagina* buscar_pagina(t_memoria_interna* mem, char* file, char* tag, int num_pagina) {
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
        // Actualizar timestamp para LRU
        encontrada->ultimo_acceso = time(NULL);
        encontrada->bit_referencia = true; // Para CLOCK-M
        
        // Actualizar en lista LRU si es necesario
        if (mem->algoritmo_reemplazo == LRU) {
            actualizar_referencia_lru(mem, encontrada);
        }
    }
    
    return encontrada;
}

// Actualizar referencia en LRU
void actualizar_referencia_lru(t_memoria_interna* mem, t_entrada_pagina* entrada) {
    // Remover de la lista si existe
    list_remove_element(mem->lru_list, entrada);
    // Agregar al final (mas reciente)
    list_add(mem->lru_list, entrada);
}

// Encontrar marco libre
int encontrar_marco_libre(t_memoria_interna* mem) {
    for (int i = 0; i < mem->cantidad_marcos; i++) {
        if (mem->marcos[i]->libre) {
            return i;
        }
    }
    return -1;
}

// Aplicar algoritmo LRU - VERSIÓN CORREGIDA
int aplicar_lru(t_memoria_interna* mem, int query_id) {
    if (list_is_empty(mem->lru_list)) {
        log_warning(mem->logger, "LRU list vacía, usando marco 0");
        return 0;
    }
    
    // La primera entrada es la menos recientemente usada
    t_entrada_pagina* victima = list_remove(mem->lru_list, 0);
    
    // Log de reemplazo
    log_info(mem->logger, "## Query<%d>: Se reemplaza la pagina %s:%s/%d", 
             query_id, victima->file, victima->tag, victima->numero_pagina);
    
    // Si esta modificada, hacer flush
    if (victima->modificada) {
        log_info(mem->logger, "Query<%d>: Pagina modificada, deberia hacer FLUSH", query_id);
    }
    
    // Remover de la tabla de páginas - CORRECCIÓN APLICADA
    char* clave = clave_file_tag(victima->file, victima->tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    
    if (tabla) {
        bool misma_entrada(t_entrada_pagina* elem) {
            return elem == victima;
        }
        list_remove_by_condition(tabla, (void*)misma_entrada);
        
        // Si la tabla queda vacía, removerla completamente
        if (list_is_empty(tabla)) {
            // ✅ CORRECCIÓN: dictionary_remove_and_destroy ya libera la clave internamente
            dictionary_remove_and_destroy(mem->tablas_paginas, clave, (void*)list_destroy);
            // NO hacer free(clave) aquí - ya se liberó internamente
        } else {
            free(clave); // Liberar solo si no removemos del dictionary
        }
    } else {
        free(clave); // Liberar si no había tabla
    }
    
    int marco_victima = victima->marco;
    
    // Liberar el marco
    mem->marcos[marco_victima]->libre = true;
    mem->marcos[marco_victima]->entrada_pagina = NULL;
    
    // Liberar entrada de pagina
    free(victima->file);
    free(victima->tag);
    free(victima);
    
    return marco_victima;
}

// Aplicar algoritmo CLOCK-M (estructura preparada para implementacion futura)
int aplicar_clock_m(t_memoria_interna* mem, int query_id) {
    // Por ahora usamos LRU como fallback
    // En CP3 se implementara CLOCK-M completo
    log_info(mem->logger, "CLOCK-M no implementado aun, usando LRU");
    return aplicar_lru(mem, query_id);
}

// Cargar una nueva pagina
int cargar_pagina(t_memoria_interna* mem, int query_id, char* file, char* tag, int num_pagina) {
    // Buscar marco libre
    int marco = encontrar_marco_libre(mem);
    
    if (marco == -1) {
        // No hay marcos libres, aplicar algoritmo de reemplazo
        if (mem->algoritmo_reemplazo == LRU) {
            marco = aplicar_lru(mem, query_id);
        } else {
            marco = aplicar_clock_m(mem, query_id);
        }
    }
    
    // Marcar marco como ocupado
    mem->marcos[marco]->libre = false;
    
    // Crear nueva entrada de pagina
    t_entrada_pagina* nueva_entrada = malloc(sizeof(t_entrada_pagina));
    nueva_entrada->file = string_duplicate(file);
    nueva_entrada->tag = string_duplicate(tag);
    nueva_entrada->numero_pagina = num_pagina;
    nueva_entrada->marco = marco;
    nueva_entrada->modificada = false;
    nueva_entrada->ultimo_acceso = time(NULL);
    nueva_entrada->bit_referencia = true;
    
    // Asignar al marco
    mem->marcos[marco]->entrada_pagina = nueva_entrada;
    
    // Registrar en tabla de paginas
    char* clave = clave_file_tag(file, tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    if (!tabla) {
        tabla = list_create();
        dictionary_put(mem->tablas_paginas, clave, tabla);
    } else {
        free(clave);
    }
    list_add(tabla, nueva_entrada);
    
    // Agregar a LRU si corresponde
    if (mem->algoritmo_reemplazo == LRU) {
        list_add(mem->lru_list, nueva_entrada);
    }
    
    // Simular lectura del Storage (en CP2 es mock, en CP3 sera real)
    void* dir_fisica = mem->memory_arena + marco * mem->tamanio_pagina;
    memset(dir_fisica, 0, mem->tamanio_pagina); // Inicializar con ceros
    
    // Logs obligatorios
    log_info(mem->logger, "Query<%d>: - Memoria Add - File:%s - Tag:%s - Pagina:%d - Marco:%d",
             query_id, file, tag, num_pagina, marco);
    log_info(mem->logger, "Query<%d>: Se asigna el Marco:%d a la Pagina:%d perteneciente al - File:%s - Tag:%s",
             query_id, marco, num_pagina, file, tag);
    
    return marco;
}

// Acceso principal: READ o WRITE
void* acceder_memoria(t_memoria_interna* mem, int query_id, char* file, char* tag, int offset, size_t tam, bool es_escritura) {
    int num_pagina = offset / mem->tamanio_pagina;
    int despl = offset % mem->tamanio_pagina;
    
    // Verificar que el acceso no se sale de la pagina
    if (despl + tam > mem->tamanio_pagina) {
        log_error(mem->logger, "Query<%d>: Acceso cruza limite de pagina", query_id);
        return NULL;
    }
    
    t_entrada_pagina* entrada = buscar_pagina(mem, file, tag, num_pagina);
    
    if (!entrada) {
        // MISS - La pagina no esta en memoria
        log_info(mem->logger, "Query<%d>: - Memoria Miss - File:%s - Tag:%s - Pagina:%d",
                 query_id, file, tag, num_pagina);
        
        int marco = cargar_pagina(mem, query_id, file, tag, num_pagina);
        entrada = buscar_pagina(mem, file, tag, num_pagina); // Ahora deberia existir
    }
    
    // Simular retardo de memoria
    usleep(mem->retardo_memoria * 1000);
    
    void* dir_fisica = mem->memory_arena + entrada->marco * mem->tamanio_pagina + despl;
    
    if (es_escritura) {
        entrada->modificada = true;
        log_info(mem->logger, "Query<%d>: Accion:ESCRIBIR - Direccion Fisica:%p", query_id, dir_fisica);
    } else {
        log_info(mem->logger, "Query<%d>: Accion:LEER - Direccion Fisica:%p", query_id, dir_fisica);
    }
    
    return dir_fisica;
}

// Inicializacion
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
    
    // Reservar memoria principal
    m->memory_arena = malloc(m->tamanio_memoria);
    memset(m->memory_arena, 0, m->tamanio_memoria);
    
    // Inicializar marcos
    m->marcos = malloc(sizeof(t_marco*) * m->cantidad_marcos);
    for (int i = 0; i < m->cantidad_marcos; i++) {
        m->marcos[i] = malloc(sizeof(t_marco));
        m->marcos[i]->numero_marco = i;
        m->marcos[i]->libre = true;
        m->marcos[i]->entrada_pagina = NULL;
    }
    
    // Inicializar estructuras de algoritmos
    m->lru_list = list_create();
    m->clock_m = NULL; // Se inicializara en CP3 si se usa CLOCK-M
    
    // Inicializar tablas de paginas
    m->tablas_paginas = dictionary_create();
    
    log_info(logger, "Memoria Interna creada: %d bytes, %d marcos, pagina=%d bytes, algoritmo=%s",
             m->tamanio_memoria, m->cantidad_marcos, m->tamanio_pagina, algoritmo_str);
    
    return m;
}

// Función para liberar una tabla de páginas de forma segura
void liberar_tabla_segura(char* key, void* value) {
    t_list* tabla = (t_list*) value;
    
    // Liberar todas las entradas de esta tabla
    for (int i = 0; i < list_size(tabla); i++) {
        t_entrada_pagina* e = list_get(tabla, i);
        if (e) {  // Verificar que no sea NULL
            free(e->file);
            free(e->tag);
            free(e);
        }
    }
    list_destroy(tabla);
    free(key);  // Liberar la clave
}

void destruir_memoria(t_memoria_interna* memoria) {
    if (!memoria) return;
    
    free(memoria->memory_arena);
    
    for (int i = 0; i < memoria->cantidad_marcos; i++) {
        free(memoria->marcos[i]);
    }
    free(memoria->marcos);
    
    // Liberar todo solo desde el dictionary
    dictionary_iterator(memoria->tablas_paginas, liberar_tabla_segura);
    dictionary_destroy(memoria->tablas_paginas);
    
    // Solo destruir la lista LRU, NO sus elementos (ya fueron liberados)
    list_destroy(memoria->lru_list);
    
    if (memoria->clock_m) {
        free(memoria->clock_m->marcos);
        free(memoria->clock_m);
    }
    
    free(memoria);
}