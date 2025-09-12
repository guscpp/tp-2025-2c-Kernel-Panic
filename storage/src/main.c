#include "storage.h"

typedef struct {
    t_log* logger;
    t_config* config;
    char* puerto;
    int fresh;
    char* punto_montaje;
    int retardo_operacion;
    int retardo_acceso_bloque;
    char* log_level;   
}storage_t;

storage_t* iniciar_storage(){
    storage_t* storage = malloc(sizeof(storage_t));

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

void liberar_storage (storage_t* storage){
    if(storage != NULL){
        log_destroy(storage->logger);
        config_destroy(storage->config);
        //free(storage->punto_montaje);
        //free(storage->log_level);
        free(storage);
    }

}

int main(int argc, char* argv[]) {
    
    storage_t* storage = iniciar_storage();

    log_info(storage->logger, "Puerto leido: %s", storage->puerto);
    log_info(storage->logger, "Fresh leido: %d", storage->fresh);
    log_info(storage->logger, "Punto de montaje leido: %s", storage->punto_montaje);
    log_info(storage->logger, "Retardo operacion leido: %d", storage->retardo_operacion);
    log_info(storage->logger, "Retardo acceso bloque leido: %d", storage->retardo_acceso_bloque);
    log_info(storage->logger, "log level leido: %s", storage->log_level);

    
    int storage_fd = iniciar_servidor(storage->puerto);  //socket, bind, listen
    log_info(storage->logger, "Servidor listo");
    int worker_fd = esperar_cliente(storage_fd);//acept
    log_info(storage->logger, "Llego un cliente"); 
    
    liberar_storage(storage);
    
    return 0;
}

/*int inicializar_file_system(storage_t* storage){
    if(storage->fresh){
        formatear_fs(storage);
    }
}




int formatear_fs(storage_t* storage){
    return 0;
    
}
*/

