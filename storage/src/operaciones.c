#include "storage.h"

bool crear_file(t_storage* storage, t_list* parametros)
{   
    if (!parametros || list_size(parametros) < 3) {
        log_error(storage->logger, "Parametros invalidos para crear_file");
        return false;
    }

    char* nombre_file = list_get(parametros, 1); 

    char* tag_inicial = list_get(parametros, 2);

    char* ruta_file = string_from_format("%s/files/%s", storage->punto_montaje, nombre_file);

    if(access(ruta_file, F_OK) == 0) { // access() con F_OK devuelve 0 si el archivo o directorio existe
        log_error(storage->logger, "El file %s ya existe", nombre_file);
        free(ruta_file);
        return false;
    }

    // Logica para crear el archivo
    log_info(storage->logger, "Creando archivo: %s", nombre_file);
    mkdir(ruta_file, 0777); // Crear el directorio del file
    
    char* ruta_tag = string_from_format("%s/%s", ruta_file, tag_inicial);
    if(access(ruta_tag, F_OK) == 0) { 
        log_error(storage->logger, "El tag inicial %s ya existe para el file %s", tag_inicial, nombre_file);
        free(ruta_tag);
        free(ruta_file);
        return false;
    }

    mkdir(ruta_tag, 0777); // Crear el directorio del tag inicial

    char* path_metadata = string_from_format("%s/metadata.config", ruta_tag);
    FILE* metadata_file = fopen(path_metadata, "w");
    if(metadata_file == NULL) {
        log_error(storage->logger, "Error al crear metadata.config para el tag inicial");
        free(ruta_tag);
        free(ruta_file);
        free(path_metadata);
        return false;
    }

    fprintf(metadata_file, "TAMANIO=0\n");
    fprintf(metadata_file, "ESTADO=WORK_IN_PROGRESS\n");
    fprintf(metadata_file, "BLOQUES=[]\n");
    fclose(metadata_file);

    usleep(storage->retardo_operacion * 1000);   //retardo requerido por el enunciado

    free(ruta_tag);
    free(ruta_file);
    free(path_metadata);

    return true;
}
