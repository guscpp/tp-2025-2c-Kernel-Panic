#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <commons/log.h>
#include <stdlib.h>

typedef struct {
    size_t    tamanio;
} t_memoria_interna;

t_memoria_interna*  memoria_crear();
void                memoria_destruir();

#endif