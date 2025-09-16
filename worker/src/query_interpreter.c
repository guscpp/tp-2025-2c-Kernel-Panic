// worker/src/query_interpreter.c
#include "../include/query_interpreter.h"
#include "../include/memoria.h"
#include "../include/worker.h"

// Dejo esto de modelo
t_query_interpreter* query_interpreter_crear() {
    t_query_interpreter* interpreter = malloc(sizeof(t_query_interpreter));
    // ...
    return interpreter;
}