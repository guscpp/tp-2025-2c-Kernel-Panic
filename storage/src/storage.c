#include "storage.h"

t_storage* iniciar_storage(){
    t_storage* storage = malloc(sizeof(t_storage));

    storage->logger = iniciar_logger("storage.log", "STORAGE", 1, LOG_LEVEL_INFO);

    storage->config = iniciar_config(storage->logger, "storage.config");

    storage->puerto = config_get_string_value(storage->config, "PUERTO_ESCUCHA");
    storage->fresh = config_get_int_value(storage->config, "FRESH_START");
    storage->punto_montaje = config_get_string_value(storage->config, "PUNTO_MONTAJE");
    storage->retardo_operacion = config_get_int_value(storage->config, "RETARDO_OPERACION");
    storage->retardo_acceso_bloque = config_get_int_value(storage->config, "RETARDO_ACCESO_BLOQUE");
    storage->log_level = config_get_string_value(storage->config, "LOG_LEVEL");

    log_info(storage->logger, "El storage se inicializo correctamente");

    return storage;
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

