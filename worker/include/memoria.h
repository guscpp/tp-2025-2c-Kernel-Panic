#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // para los usleep

typedef enum {
    LRU,
    CLOCK_M
} t_algoritmo_reemplazo;

// Entrada de una página en la tabla
typedef struct {
    char* file;
    char* tag;
    int numero_pagina;
    int marco;
    bool modificada;
} entrada_pagina_t;

// Estructura principal de la Memoria Interna
typedef struct {
    int tamanio_memoria;           // TAM_MEMORIA del config
    int tamanio_pagina;            // BLOCK_SIZE (viene del Storage)
    int cantidad_marcos;           // = tamanio_memoria / tamanio_pagina
    t_algoritmo_reemplazo algoritmo_reemplazo;
    int retardo_memoria;           // en ms
    t_log* logger;
    void* memory_arena;            // malloc único

    // Estado de marcos (para reemplazo simple en CP2)
    bool* marco_ocupado;

    // Tablas de páginas: clave = "file:tag", valor = t_list* de entrada_pagina_t*
    t_dictionary* tablas_paginas;
} t_memoria_interna;

// Funciones públicas
t_memoria_interna* crear_memoria(t_log* logger, int tam_memoria, int retardo_memoria, char* algoritmo_str, int block_size);
void destruir_memoria(t_memoria_interna* memoria);

// Acceso principal desde el Query Interpreter
void* acceder_memoria(t_memoria_interna* mem, int query_id, char* file, char* tag, int offset, size_t tam, bool es_escritura);

#endif