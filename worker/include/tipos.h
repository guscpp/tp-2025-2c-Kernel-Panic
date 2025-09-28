#ifndef TIPOS_H_
#define TIPOS_H_

#include "../../utils/include/utils.h"
#include "memoria.h"  
#include "query_interpreter.h"


// Struct modelo: borrala, cambiala o lo que haga falta

typedef struct {
    t_memoria_interna* memoria;
    int pc;
    bool hay_interrupcion; 
} t_query_interpreter;
//--------------------------------
typedef struct
{
    t_log*              logger;
    t_config*           config;
    char*               ip_master;
    char*               puerto_master;
    char*               ip_storage;
    char*               puerto_storage;
    char*               path_scripts;
    char*               log_level;
    int                 id_worker;
    int                 storage_socket;
    int                 master_socket;
    pthread_t           thread_master; 
    pthread_t           thread_storage; 
    t_memoria_interna*  mem;
    t_query_interpreter* interpreter;

} t_worker;

typedef struct {
    int master_socket;
    t_worker* w;

} t_ejecucion;

//EN QUERY INTERPRETER


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
    bool fin; //true -> es el END
    t_instr_param* parametros;
    void (*ejecuta_instruccion)(t_instr_param*);
}t_decode; //solo para que haya execute

typedef struct{
    FILE* archivo;
    char* nombre_archivo;
    int query_id;
    int pc; //con el que viene desde kernel
} Pcb;

#endif
