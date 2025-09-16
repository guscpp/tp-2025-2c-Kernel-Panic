#ifndef QUERY_INTERPRETER_H_
#define QUERY_INTERPRETER_H_

#include "memoria.h"
#include "worker.h"

// Struct modelo: borrala, cambiala o lo que haga falta
typedef struct {
    t_memoria_interna* memoria;
    // ... otros campos ...
} t_query_interpreter;

// Modelo para que haya algo, 
t_query_interpreter*    query_interpreter_crear();

#endif