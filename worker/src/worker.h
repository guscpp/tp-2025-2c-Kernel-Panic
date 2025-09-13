#ifndef WORKER_H_
#define WORKER_H_

#include "../../utils/include/utils.h"

typedef struct
{
    t_log*    logger;
    t_config* config;
    char*     ip_master;
    char*     puerto_master;
    char*     ip_storage;
    char*     puerto_storage;
    int       tamanio_memoria;
    int       retardo_memoria;
    char*     algoritmo_reemplazo;
    char*     path_scripts;
    char*     log_level;
} t_worker;

t_worker* inicializar_worker();
void verificar_worker(t_worker* w);

#endif /* CLIENTE_H_ */