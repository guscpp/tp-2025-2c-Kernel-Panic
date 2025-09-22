#ifndef QUERY_INTERPRETER_H_
#define QUERY_INTERPRETER_H_

#include "memoria.h"
#include "worker.h"

// Struct modelo: borrala, cambiala o lo que haga falta
typedef struct {
    t_memoria_interna* memoria;
} t_query_interpreter;

typedef struct{
    FILE* archivo;
    char* nombre_archivo;
    int query_id;
    int pc;
} PCB;

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
    t_instr_param* parametros;
    void (*ejecuta_instruccion)(t_instr_param*);
}t_decode; //solo para que haya execute

// Modelo para que haya algo, 
t_query_interpreter*    query_interpreter_crear();
void query_interpreter_ciclo(FILE* archivo_query, t_worker* w);
#endif