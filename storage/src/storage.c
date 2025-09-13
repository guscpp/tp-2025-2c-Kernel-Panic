#include "storage.h"

t_storage* iniciar_storage(){
    t_storage* storage = malloc(sizeof(t_storage));

    storage->logger = iniciar_logger("storage.log", "STORAGE", true, LOG_LEVEL_INFO);
    storage->config = iniciar_config(storage->logger, "storage.config");
    storage->puerto_escucha = config_get_string_value(storage->config, "PUERTO_ESCUCHA");
    storage->fresh_start = config_get_int_value(storage->config, "FRESH_START");
    storage->punto_montaje = config_get_string_value(storage->config, "PUNTO_MONTAJE");
    storage->retardo_operacion = config_get_int_value(storage->config, "RETARDO_OPERACION");
    storage->retardo_acceso_bloque = config_get_int_value(storage->config, "RETARDO_ACCESO_BLOQUE");
    storage->log_level = config_get_string_value(storage->config, "LOG_LEVEL");

    log_info(storage->logger, "El storage se inicializo correctamente");

    return storage;
}

void verificar_storage(t_storage* s)
{
    log_info(s->logger, "Puerto leido: %s", s->puerto_escucha);
    log_info(s->logger, "Fresh leido: %d", s->fresh_start);
    log_info(s->logger, "Punto de montaje leido: %s", s->punto_montaje);
    log_info(s->logger, "Retardo operacion leido: %d", s->retardo_operacion);
    log_info(s->logger, "Retardo acceso bloque leido: %d", s->retardo_acceso_bloque);
    log_info(s->logger, "log level leido: %s", s->log_level);
}

void liberar_storage (t_storage* storage){
    if(storage != NULL){
        log_destroy(storage->logger);
        config_destroy(storage->config);
        //free(storage->punto_montaje);
        //free(storage->log_level);
        free(storage);
    }

}


/*int inicializar_file_system(t_storage* storage){
    if(storage->fresh){
        formatear_fs(storage);
    }
}




int formatear_fs(t_storage* storage){
    return 0;
    
}
*/

