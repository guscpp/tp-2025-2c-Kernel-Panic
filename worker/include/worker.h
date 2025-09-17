#ifndef WORKER_H_
#define WORKER_H_

#include "../../utils/include/utils.h"
#include "memoria.h"
#include "query_interpreter.h"

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

} t_worker;

t_worker* inicializar_worker(int id_worker);

void      verificar_worker(t_worker* worker);
void      liberar_worker(t_worker* w);
void      recibir_path_de_query(int master_socket, t_log* logger);

void rtas_storage(int storage_socket, t_worker* w);

#endif /* CLIENTE_H_*/