
#include "memoria.h"
#include <commons/string.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../../utils/include/utils.h"
#include <semaphore.h>


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
/*
int cargar_pagina(t_memoria_interna* mem, int query_id, char* file, char* tag, int num_pagina) {
    int marco = encontrar_marco_libre(mem);

    if (marco == -1) {
        if (mem->algoritmo_reemplazo == LRU)
            marco = aplicar_lru(mem, query_id);
        else
            marco = aplicar_clock_m(mem, query_id);
    }

    // 2. Pedís el bloque a Storage
    if (pedir_bloque_storage(mem, query_id, file, tag, pagina) < 0) {
        log_error(mem->logger, "Error cargando página %d", pagina);
        return -1;
    }

    
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
*/

int cargar_pagina(t_memoria_interna* mem, int query_id, char* file, char* tag, int num_pagina) { //esta es la nueva, le agregue que copie lo que traae del bloque logico
    int marco = encontrar_marco_libre(mem);

    if (marco == -1) {
        if (mem->algoritmo_reemplazo == LRU)
            marco = aplicar_lru(mem, query_id);
        else
            marco = aplicar_clock_m(mem, query_id);
    }

    //pedir el bloque ANTES de registrar la página
    if (pedir_bloque_storage(mem, query_id, file, tag, num_pagina) < 0) {
        log_error(mem->logger, "Error cargando pagina: %d", num_pagina);
        return -1;
    }
    
    // Copiar contenido recibido desde storage al marco de memoria interna
    void* destino = mem->memory_arena + marco * mem->tamanio_pagina;
    memcpy(destino, mem->tmp_bloque, mem->tamanio_pagina);
    free(mem->tmp_bloque);
    mem->tmp_bloque = NULL;
    
    // NO usar memset a 0
    //memset(destino, 0, mem->tamanio_pagina);

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

    // Logs correctos
    log_info(mem->logger, "Query<%d>: - Memoria Add - File:%s - Tag:%s - Pagina:%d - Marco:%d",
             query_id, file, tag, num_pagina, marco);
    log_info(mem->logger, "Query<%d>: Se asigna el Marco:%d a la Pagina:%d perteneciente al - File:%s - Tag:%s",
             query_id, marco, num_pagina, file, tag);

    return marco;
}

void* acceder_memoria(t_memoria_interna* mem, int query_id, char* file, char* tag, int offset, size_t tam, bool es_escritura) {
    if (!mem || !file || !tag || tam == 0) return NULL;
    
    int num_pagina_inicial = offset / mem->tamanio_pagina;
    int despl_inicial = offset % mem->tamanio_pagina;
    int num_pagina_final = (offset + tam - 1) / mem->tamanio_pagina;
    // int paginas_necesarias = num_pagina_final - num_pagina_inicial + 1; // Variable no usada, se elimina para evitar advertencia
    
    // 1. Verificar limites del archivo (consultando metadata)
    char* metadata_path = string_from_format("%s/files/%s/%s/metadata.config",
                                            "/home/utnso/TP_SSOO/tp-2025-2c-Kernel-Panic/storage", // Punto de montaje hardcodeado temporalmente
                                            file, tag);
    if (access(metadata_path, F_OK) != 0) {
        log_error(mem->logger, "Query<%d>: No se pudo cargar metadata de <%s>:<%s>", query_id, file, tag);
        free(metadata_path);
        return NULL;
    }
    
    t_config* metadata = config_create(metadata_path);
    if (!metadata) {
        log_error(mem->logger, "Query<%d>: No se pudo cargar metadata de <%s>:<%s>", query_id, file, tag);
        free(metadata_path);
        return NULL;
    }
    
    int tam_archivo = config_get_int_value(metadata, "TAMANIO");
    config_destroy(metadata);
    free(metadata_path);
    
    if (offset + tam > tam_archivo) {
        log_error(mem->logger, "Query<%d>: Acceso fuera de limite - Offset: %d, Tamanio: %zu, Tamanio archivo: %d",
                 query_id, offset, tam, tam_archivo);
        error_tamanio_escrLectura_excedido(mem->logger, 
                                         es_escritura ? WORKER_ERROR_TAMANIO_ESCRITURA_EXCEDIDO : WORKER_ERROR_TAMANIO_LECTURA_EXCEDIDO,
                                         query_id, file, tag);
        return NULL;
    }
    
    // Buffer temporal para almacenar el contenido si es lectura
    void* buffer_total = NULL;
    if (!es_escritura) {
        buffer_total = malloc(tam);
        if (!buffer_total) {
            log_error(mem->logger, "Query<%d>: Error al allocar buffer para operacion multi-bloque", query_id);
            return NULL;
        }
    }
    
    // Procesar cada pagina necesaria
    size_t bytes_restantes = tam;
    size_t bytes_procesados = 0;
    int pagina_actual = num_pagina_inicial;
    int despl_actual = despl_inicial;
    
    while (bytes_restantes > 0 && pagina_actual <= num_pagina_final) {
        // Calcular cuantos bytes procesar en esta pagina
        size_t bytes_en_pagina = (despl_actual + bytes_restantes > mem->tamanio_pagina) ?
                                (mem->tamanio_pagina - despl_actual) : bytes_restantes;
        
        // Asegurar que la pagina esta cargada en memoria
        t_entrada_pagina* entrada = buscar_pagina(mem, file, tag, pagina_actual);
        if (!entrada) {
            log_info(mem->logger, "Query<%d>: - Memoria Miss - File:%s - Tag:%s - Pagina:%d",
                    query_id, file, tag, pagina_actual);
            int marco = cargar_pagina(mem, query_id, file, tag, pagina_actual);
            if (marco == -1) {
                log_error(mem->logger, "Query<%d>: Error al cargar pagina %d", query_id, pagina_actual);
                if (buffer_total) free(buffer_total);
                return NULL;
            }
            entrada = buscar_pagina(mem, file, tag, pagina_actual);
            if (!entrada) {
                log_error(mem->logger, "Query<%d>: Error al obtener entrada de pagina %d despues de cargarla", 
                         query_id, pagina_actual);
                if (buffer_total) free(buffer_total);
                return NULL;
            }
        }
        
        // Actualizar estado de la pagina segun la operacion
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
        
        // Calcular direccion fisica
        void* dir_fisica = mem->memory_arena + entrada->marco * mem->tamanio_pagina + despl_actual;
        
        // Aplicar retardo
        usleep(mem->retardo_memoria * 1000);
        
        if (es_escritura) {
            // Para escritura: copiar los bytes correspondientes
            char* contenido_parcial = ((char*)(((t_instr_param*)mem->memoria_contexto)->contenido)) + bytes_procesados;
            memcpy(dir_fisica, contenido_parcial, bytes_en_pagina);
            
            // Log de escritura parcial
            char* valor_str = string_substring(dir_fisica, 0, bytes_en_pagina);
            log_debug(mem->logger, "Query<%d>: Accion:ESCRIBIR - Direccion Fisica:%p - Valor:%s (pagina %d, bytes: %zu)",
                    query_id, dir_fisica, valor_str, pagina_actual, bytes_en_pagina);
            free(valor_str);
        } else {
            // Para lectura: copiar los bytes al buffer total
            memcpy((char*)buffer_total + bytes_procesados, dir_fisica, bytes_en_pagina);
            
            // Log de lectura parcial
            char* valor_str = string_substring(dir_fisica, 0, bytes_en_pagina);
            log_debug(mem->logger, "Query<%d>: Accion:LEER - Direccion Fisica:%p - Valor:%s (pagina %d, bytes: %zu)",
                    query_id, dir_fisica, valor_str, pagina_actual, bytes_en_pagina);
            free(valor_str);
        }
        
        // Actualizar contadores
        bytes_restantes -= bytes_en_pagina;
        bytes_procesados += bytes_en_pagina;
        pagina_actual++;
        despl_actual = 0; // En las paginas siguientes, empezamos desde el byte 0
    }
    
    // Retornar el buffer total para lecturas
    if (!es_escritura) {
        return buffer_total;
    }
    
    return (void*)1; // Indicador de exito para escrituras
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

// devuelve 0 en éxito (y copia el bloque dentro del marco en cargar_pagina),
// devuelve -2 si hubo error de storage / red.
int pedir_bloque_storage(t_memoria_interna* mem, int query_id, char* file, char* tag, int num_pagina) { //pide un bloque logico, lo cual esta bien porque solo se busca uno cuando tira pageFAult
    if (!mem) return -2;
    int sock = mem->socket_storage;
    if (sock <= 0) {
        log_error(mem->logger, "No hay socket de storage en la estructura de memoria");
        return -2;
    }

    // 1) armar y enviar paquete de lectura de bloque lógico
    t_buffer* buffer = crear_buffer();
    t_paquete* p = crear_paquete(STORAGE_READ_BLOCK, buffer);

    agregar_a_paquete(p, &query_id, sizeof(int));
    agregar_a_paquete(p, file, strlen(file) + 1);
    agregar_a_paquete(p, tag, strlen(tag) + 1);
    agregar_a_paquete(p, &num_pagina, sizeof(int));

    enviar_paquete(p, sock, mem->logger);
    eliminar_paquete(p);

    // 2) esperar respuesta
    t_list* resp = recibir_paquete(sock);
    if (!resp || list_size(resp) < 1) {
        log_error(mem->logger, "Query<%d>: Error recibiendo respuesta de Storage para %s:%s/%d",
                  query_id, file, tag, num_pagina);
        return -2; // poner -2
    }

    int codigo = *(int*)list_get(resp, 0);
    log_info(mem->logger, "ESte es el codigo que llega %d", codigo);

    if (codigo == STORAGE_SEND_OK_READ_BLOCK) {
        // según tu servidor, el payload 1 es el contenido binario del bloque
        if (list_size(resp) < 2) {
            log_error(mem->logger, "Query<%d>: STORAGE_SEND_OK sin contenido para %s:%s/%d",
                      query_id, file, tag, num_pagina);
        }
        void* contenido_remoto = list_get(resp, 1);
        if (!contenido_remoto) {
            log_error(mem->logger, "Query<%d>: STORAGE_SEND_OK sin payload", query_id);
            list_destroy_and_destroy_elements(resp, free);
            return -2;
        }
        // Copiamos el contenido remoto a un buffer propio
        void* bloque = malloc(mem->tamanio_pagina);
        if (!bloque) {
            log_error(mem->logger, "Query<%d>: no se pudo allocar buffer para bloque", query_id);
            list_destroy_and_destroy_elements(resp, free);
            return -2;
        }
        memcpy(bloque, contenido_remoto, mem->tamanio_pagina);

        // liberamos la lista de paquete recibido (libera payloads)
        list_destroy_and_destroy_elements(resp, free);

        // Ahora devolvemos el bloque *colocándolo* en la arena en quien pidió:
        // Para esto devolvemos el bloque al caller vía un buffer global temporal
        // pero simpler: el caller (cargar_pagina) pedirá y luego copiará al marco.
        // Aquí retornamos el bloque como señal positiva usando un pointer global no ideal.
        // Para mantenerlo simple: guardamos el bloque en mem->tmp_bloque y devolvemos 0.
        // (Agregá tmp_bloque en t_memoria_interna).

        // A) si ya existe mem->tmp_bloque, liberalo
        if (mem->tmp_bloque) { free(mem->tmp_bloque); mem->tmp_bloque = NULL; }
        mem->tmp_bloque = bloque; //y le asigno a bloque temporal el bloque que se trajo de storage
        return 0;
    } else {
        // Storage reportó error. PODRIA SER ESTE EL ERROR DE ENTRAR A UNA PAGINA QUE NO LE CORRESPONDE
        log_error(mem->logger, "Query<%d>: Storage respondió con error al pedir bloque %s:%s/%d",
                  query_id, file, tag, num_pagina);
        list_destroy_and_destroy_elements(resp, free);
        return -2; //poner -2
    }
}


/*
void flush_paginas_modificadas( //mando a storage la cantidad de paginas modificadas. LAs mando todas juntas en un solo paquete
    t_memoria_interna* mem,
    int query_id,
    char* file,
    char* tag,
    int socket_storage
) {
    char* clave = clave_file_tag(file, tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    free(clave);
    if (!tabla) return;

    t_buffer* buffer = crear_buffer();
    t_paquete* p = crear_paquete(STORAGE_FLUSH, buffer);

    agregar_a_paquete(p, &query_id, sizeof(int));
    agregar_a_paquete(p, file, strlen(file)+1);
    agregar_a_paquete(p, tag, strlen(tag)+1);

    int cantidad_paginas_modificadas = 0;

    for (int i = 0; i < tabla->elements_count; i++) {
        t_entrada_pagina* e = list_get(tabla, i);
        if (e->modificada) {
            cantidad_paginas_modificadas++;

            agregar_a_paquete(p, &e->numero_pagina, sizeof(int));

            void* contenido = mem->memory_arena + e->marco * mem->tamanio_pagina;
            agregar_a_paquete(p, contenido, mem->tamanio_pagina);

            e->modificada = false; // reset
            if (mem->algoritmo_reemplazo == CLOCK_M)
                mem->clock_m->bits_modificados[e->marco] = false;
        }
    }

    agregar_a_paquete(p, &cantidad_paginas_modificadas, sizeof(int));

    enviar_paquete(p, socket_storage, mem->logger);
    eliminar_paquete(p);

    log_info(mem->logger,
        "Query<%d>: FLUSH de %d páginas modificadas del File:%s Tag:%s",
        query_id, cantidad_paginas_modificadas, file, tag
    );
}
*/
//este es como el de arriba solo que con logs
void flush_paginas_modificadas( t_memoria_interna* mem, int query_id, char* file, char* tag, int socket_storage) {
    char* clave = clave_file_tag(file, tag);
    t_list* tabla = dictionary_get(mem->tablas_paginas, clave);
    free(clave);

    if (!tabla) {
        log_warning(mem->logger,
            "Query<%d>: FLUSH - No hay páginas cargadas de %s:%s",
            query_id, file, tag
        );
        return;
    }

    t_buffer* buffer = crear_buffer();
    t_paquete* p = crear_paquete(STORAGE_FLUSH, buffer);

    agregar_a_paquete(p, &query_id, sizeof(int));
    agregar_a_paquete(p, file, strlen(file)+1);
    agregar_a_paquete(p, tag, strlen(tag)+1);

    int cantidad_paginas_modificadas = 0;

    for (int i = 0; i < tabla->elements_count; i++) {
        t_entrada_pagina* e = list_get(tabla, i);
        if (e->modificada) {
            cantidad_paginas_modificadas++;

            void* contenido = mem->memory_arena + e->marco * mem->tamanio_pagina;

            // LOG DE DEPURACIÓN: datos de la página que se mandará
            log_info(mem->logger,
                "Query<%d>: FLUSH -> Pagina:%d | Marco:%d | DirFisica:%p | Tam:%d bytes",
                query_id,
                e->numero_pagina,
                e->marco,
                contenido,
                mem->tamanio_pagina
            );

            // Mostrar una vista del contenido (solo si es imprimible)
            char* vista = string_substring(contenido, 0, mem->tamanio_pagina);
            log_info(mem->logger,
                "Query<%d>: Contenido enviado Página:%d --> \"%s\"",
                query_id,
                e->numero_pagina,
                vista
            );
            free(vista);

            // Agregar al paquete
            agregar_a_paquete(p, &e->numero_pagina, sizeof(int));
            agregar_a_paquete(p, contenido, mem->tamanio_pagina);

            // Reset modificado
            e->modificada = false;
            if (mem->algoritmo_reemplazo == CLOCK_M)
                mem->clock_m->bits_modificados[e->marco] = false;
        }
    }

    agregar_a_paquete(p, &cantidad_paginas_modificadas, sizeof(int));

    // enviar a Storage
    enviar_paquete(p, socket_storage, mem->logger);
    eliminar_paquete(p);

    log_info(mem->logger,
        "Query<%d>: FLUSH completado - %d páginas enviadas - File:%s Tag:%s",
        query_id,
        cantidad_paginas_modificadas,
        file,
        tag
    );
}
