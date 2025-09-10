#include "storage.h"



int main(int argc, char* argv[]) {
    t_log* logger;
    int puerto;
    bool fresh_start;
    char* punto_montaje;
    int retardo_operacion;
    int retardo_acceso_bloque;
    char* log_level;
    

    logger = iniciar_logger("storage.log", "STORAGE", true, LOG_LEVEL_INFO);
    log_info(logger, "Hola soy un storage");

    t_config* config = iniciar_config(logger, "storage.config");

    puerto = config_get_int_value(config, "PUERTO_ESCUCHA");
    fresh_start = true;
    punto_montaje = config_get_string_value(config, "PUNTO_MONTAJE");
    retardo_operacion = config_get_int_value(config, "RETARDO_OPERACION");
    retardo_acceso_bloque = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");
    log_level = config_get_string_value(config, "LOG_LEVEL");

    log_info(logger,"PUERTO_ESCUCHA: %d ", puerto);
    log_info(logger, "FRESH_START: %s", fresh_start ? "TRUE" : "FALSE");
    log_info(logger,"PUNTO_MONTAJE: %s", punto_montaje);
    log_info(logger,"RETARDO_OPERACION: %d", retardo_operacion);
    log_info(logger, "RETARDO_ACCESO_BLOQUE: %d", retardo_acceso_bloque );
    log_info(logger, "LOG_LEVEL: %s", log_level);
    
    
    // error en la config (no se crean los archivos. config) 
}