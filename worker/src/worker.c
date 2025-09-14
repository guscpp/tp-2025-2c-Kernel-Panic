#include "worker.h"
#include <unistd.h>

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

    // w->master_socket = NULL;
    // w->storage_socket = NULL;

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

void procesar_asignacion_query(int master_socket, t_log* logger)
{
    while (1)
    {
        int codigo_operacion = recibir_operacion(master_socket);
        
        if (codigo_operacion == -1) {
            log_error(logger, "Error en la conexión con el Master");
            break;
        }
        
        if (codigo_operacion == WORKER_ASSIGN_QUERY)
        {
            // Recibir información de la query
            int size_path;
            recv(master_socket, &size_path, sizeof(int), MSG_WAITALL);
            
            char* path_query = malloc(size_path);
            recv(master_socket, path_query, size_path, MSG_WAITALL);
            
            int prioridad;
            recv(master_socket, &prioridad, sizeof(int), MSG_WAITALL);
            
            log_info(logger, "Query asignada: %s (prioridad: %d)", path_query, prioridad);
            
            // TODO: Codigo para procesar la query
            
            free(path_query);
            break;
        }
    }
}

void liberar_worker(t_worker* w)
{
    if (w) {
        // if (w->master_socket != NULL) close(w->master_socket);
        // if (w->storage_socket != NULL) close(w->storage_socket);
        if (w->logger) log_destroy(w->logger);
        if (w->config) config_destroy(w->config);
        free(w);
    }
}

// Esta funcion puede adaptarse despues del checkpoint 1 para
// procesar queries, no solo recibir y mandar paths al log
void recibir_path_de_query(int master_socket, t_log* logger)
{
    int codigo_operacion = recibir_operacion(master_socket);

    if (codigo_operacion == -1)
    {
        log_error(logger, "Error en la conexión con el Master");
        return;
    }

    if (codigo_operacion == WORKER_ASSIGN_QUERY)
    {
        t_list* valores = recibir_paquete(master_socket);
        
        if (valores && list_size(valores) >= 3)
        {
            // Los valores vienen en el orden:
            // query_id, path_query, prioridad
            int query_id = *(int*)list_get(valores, 0);
            char* path_query = (char*)list_get(valores, 1);
            int prioridad = *(int*)list_get(valores, 2);
            
            log_info(logger, "Query recibida: ID=%d, Path=%s, Prioridad=%d", 
                    query_id, path_query, prioridad);
            
            // Liberar la lista (pero no los elementos)
            list_destroy(valores);
            
            free(path_query);
        }
    }
}