#include "worker.h"

t_worker* inicializar_worker()
{
    t_worker *w = malloc(sizeof(t_worker));

    // TODO leer log_level del config
    w->logger = iniciar_logger("worker.log", "[WORKER_PRUEBA]", true, LOG_LEVEL_INFO);

    w->config = iniciar_config(w->logger, "worker.config");
    w->ip_master = config_get_string_value(w->config, "IP_MASTER");
    w->puerto_master = config_get_string_value(w->config, "PUERTO_MASTER");
    w->ip_storage = config_get_string_value(w->config, "IP_STORAGE");
    w->puerto_storage = config_get_string_value(w->config, "PUERTO_STORAGE");
    w->tamanio_memoria = config_get_int_value(w->config, "TAM_MEMORIA");
    w->retardo_memoria = config_get_int_value(w->config, "RETARDO_MEMORIA");
    w->algoritmo_reemplazo = config_get_string_value(w->config, "ALGORITMO_REEMPLAZO");
    w->path_scripts = config_get_string_value(w->config, "PATH_QUERIES");
    w->log_level = config_get_string_value(w->config, "LOG_LEVEL");

    return w;
}

void verificar_worker(t_worker* w)
{
    log_info(w->logger, "Ip_Master: %s", w->ip_master);
    log_info(w->logger, "Puerto Master: %s", w->puerto_master);
    log_info(w->logger, "Ip_Storage: %s", w->ip_storage);
    log_info(w->logger, "Puerto_Storage: %s", w->puerto_storage);
    log_info(w->logger, "Tam_memoria: %d", w->tamanio_memoria);
    log_info(w->logger, "Retardo_Memoria: %d", w->retardo_memoria);
    log_info(w->logger, "Algoritmos_reemplazo: %s", w->algoritmo_reemplazo);
    log_info(w->logger, "PathScript: %s", w->path_scripts);
}