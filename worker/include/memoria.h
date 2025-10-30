#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // para los usleep
#include <time.h>   // para los timestamps en LRU

typedef enum {
    LRU,
    CLOCK_M
} t_algoritmo_reemplazo;

// Entrada de una pagina en la tabla
typedef struct {
    char* file;
    char* tag;
    int numero_pagina;
    int marco;
    bool modificada;
    time_t ultimo_acceso;  // Para LRU
    bool bit_referencia;   // Para CLOCK-M
} t_entrada_pagina;

// Marco de memoria
typedef struct {
    int numero_marco;
    bool libre;
    t_entrada_pagina* entrada_pagina;  // Pagina en este marco
} t_marco;

// Para algoritmo CLOCK-M
typedef struct {
    t_marco** marcos;
    int puntero;
    int cantidad_marcos;
    bool* bits_referencia;  // Array de bits de referencia (R)
    bool* bits_modificados; // Array de bits de modificación (M)
} t_clock_m;

// Estructura principal de la Memoria Interna
typedef struct {
    int tamanio_memoria;     // TAM_MEMORIA del config en bytes
    int tamanio_pagina;      // BLOCK_SIZE (viene del Storage) en bytes
    int cantidad_marcos;     // = tamanio_memoria / tamanio_pagina
    t_algoritmo_reemplazo algoritmo_reemplazo;
    int retardo_memoria;     // en ms
    t_log* logger;
    void* memory_arena;      // malloc único
    t_marco** marcos;        // Estado de marcos
    t_list* lru_list;        // Lista para LRU (entradas ordenadas por uso)
    t_clock_m* clock_m;      // Estructura para CLOCK-M
    t_dictionary* tablas_paginas;  // Clave = "file:tag", valor = t_list* de entrada_pagina_t*
} t_memoria_interna;

// Funciones publicas
t_memoria_interna* crear_memoria(t_log* logger, int tam_memoria, int retardo_memoria, char* algoritmo_str, int block_size);
void destruir_memoria(t_memoria_interna* memoria);

// Acceso principal desde el Query Interpreter
void* acceder_memoria(t_memoria_interna* mem, int query_id, char* file, char* tag, int offset, size_t tam, bool es_escritura);

// Funciones para los algoritmos de reemplazo
int encontrar_marco_libre(t_memoria_interna* mem);
int aplicar_lru(t_memoria_interna* mem, int query_id);
int aplicar_clock_m(t_memoria_interna* mem, int query_id);
void actualizar_referencia_lru(t_memoria_interna* mem, t_entrada_pagina* entrada);

// Funciones auxiliares
char* clave_file_tag(char* file, char* tag);
t_entrada_pagina* buscar_pagina(t_memoria_interna* mem, char* file, char* tag, int num_pagina);
int cargar_pagina(t_memoria_interna* mem, int query_id, char* file, char* tag, int num_pagina);

// Funciones CLOCK-M
t_clock_m* crear_clock_m(int cantidad_marcos, t_marco** marcos);
void destruir_clock_m(t_clock_m* clock);

#endif