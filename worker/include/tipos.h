#ifndef TIPOS_H_
#define TIPOS_H_

#include "../../utils/include/utils.h"
#include <semaphore.h>
//NO puede ir nunca ningun include de submodulo.h, porque aca NO hay funciones que necesiten parametros de otros modulos
 
typedef struct t_memoria_interna t_memoria_interna;

// Struct modelo: borrala, cambiala o lo que haga falta

typedef struct {
    t_memoria_interna* memoria;
    int pc;
    bool hay_interrupcion;
} t_query_interpreter;
//--------------------------------
typedef struct t_worker 
{
    t_log*               logger;
    t_config*            config;
    char*                ip_master;
    int                  puerto_master;
    char*                ip_storage;
    char*                puerto_storage;
    char*                path_scripts;
    char*                log_level;
    int                  id_worker;
    int                  storage_socket;
    int                  master_socket_distpach;
    int                  master_socket_interrupt;
    pthread_t            thread_master; 
    pthread_t            thread_storage; 
    t_memoria_interna*   mem;
    t_query_interpreter* interpreter;
    int                  tamanio_bloque;
    int                  cantidad_bloques;
    bool                 error_memoria;
} t_worker;

typedef struct {
    int master_socket;
    t_worker* w;

} t_ejecucion;


//EN QUERY INTERPRETER

//extern int hay_interrupcion;

typedef struct{
    char* nomb_instr;
    char* nombre_file;
    char* tag;
    int tamanio;
    int direccion_base;
    char* contenido;
    char* nombre_file_org;
    char* tag_origen;
    char* nombre_file_destino;
    char* tag_destino;
} t_instr_param;

typedef struct{
    FILE* archivo;
    char* nombre_archivo;
    int query_id;
    int pc; //con el que viene desde kernel
} Pcb;

typedef struct{
    bool fin; //true -> es el END
    t_instr_param* parametros;
    void (*ejecuta_instruccion)(t_instr_param*, t_worker*, Pcb*);
    bool instruccion_malformada; 
}t_decode; //solo para que haya execute

extern pthread_mutex_t mutex_interrupt;

extern pthread_mutex_t mutex_error_memoria;

#endif