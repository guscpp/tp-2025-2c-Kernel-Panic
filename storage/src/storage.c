#include "storage.h"

char* PATH_BASE;

t_storage* iniciar_storage(){
    t_storage* storage = malloc(sizeof(t_storage));

    storage->logger = iniciar_logger("storage.log", "STORAGE", 1, LOG_LEVEL_INFO);

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

void liberar_storage (t_storage* storage){
    if(storage != NULL){
        log_destroy(storage->logger);
        config_destroy(storage->config);
        free(storage);
    }

}

int rm_rf (char* path){
    char* comando = string_from_format("rm -rf %s", path);
    int aux = system(comando);
    free(comando);
    return aux;
}

char* obtener_ruta_absoluta(char* ruta_rel){
    char* aux = string_new();
    string_append(&aux, PATH_BASE);
    string_append(&aux, "/");
    string_append(&aux, ruta_rel);
    return aux;
}

void crear_archivos(char* ruta, char* modo){
    char* ruta_abs = obtener_ruta_absoluta(ruta);
    FILE* archivo = fopen(ruta_abs, modo);
    if(archivo != NULL){
        fclose(archivo);
    }
    free(ruta_abs);
}

void crear_directorios(char* ruta_rel){
    char* ruta_abs = obtener_ruta_absoluta(ruta_rel);
    mkdir(ruta_abs, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    free(ruta_abs);
}


void formatear_fs(){
    char* path_hash = obtener_ruta_absoluta("blocks_hash_index.config");
    char* path_bmap = obtener_ruta_absoluta("bitmap.bit");
    char* path_files = obtener_ruta_absoluta("files");
    char* path_phblck = obtener_ruta_absoluta("physical_blocks");

    rm_rf(path_hash);
    rm_rf(path_bmap);
    rm_rf(path_files);
    rm_rf(path_phblck);

    free(path_hash);
    free(path_bmap);
    free(path_files);
    free(path_phblck);

    crear_directorios("files");
    crear_directorios("physical_blocks");

    crear_archivos("bitmap.bit", "wb");
    crear_archivos("blocks_hash_index.config", "w");
    
}

bool inicializar_file_system(t_storage* storage){
    log_info(storage->logger, "Inicializando File System...");

    PATH_BASE = config_get_string_value(storage->config, "PUNTO_MONTAJE");

    
    if(storage->fresh_start){
        log_info(storage->logger, "FRESH_START=TRUE → Formateo inicial");
        formatear_fs();
    }else{
        log_info(storage->logger, "RESH_START=FALSE → Levantar estructura existente (falta implementar)");
    }

    return true;
}












