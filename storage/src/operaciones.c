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

void truncar_file(t_storage* storage, t_list* parametros)
{
    if (!parametros || list_size(parametros) < 4) {
        log_error(storage->logger, "Parametros invalidos para truncar_file");
        return false;
    }

    char* nombre_file = list_get(parametros, 1);
    char* tag = list_get(parametros, 2);
    int nuevo_tamanio = *(int*) list_get(parametros, 3);

    char* ruta_tag = string_from_format("%s/files/%s/%s/metadata.config", storage->punto_montaje, nombre_file, tag);
    char* ruta_metadata = string_from_format("%s/metadata.config", ruta_tag);

    if(access(ruta_metadata, F_OK) != 0) {
        log_error(storage->logger, "El tag %s del file %s no existe", tag, nombre_file);
        free(ruta_tag);
        free(ruta_metadata);
        return false;
    }

    t_config* metadata_config = config_create(ruta_metadata);
    if(!metadata_config){
        log_error(storage->logger, "No se pudo abrir el metadata.config para truncar el file %s tag %s", nombre_file, tag);
        free(ruta_tag);
        free(ruta_metadata);
        return false;
    }

    int tamanio_actual = config_get_int_value(metadata_config, "TAMANIO");
    char** bloques = config_get_array_value(metadata_config, "BLOQUES");

    int tamanio_bloque = storage->tamanio_bloque;
    int bloques_actuales = (int) ceil((double)tamanio_actual / tamanio_bloque);
    int bloques_nuevos = (int) ceil((double)nuevo_tamanio / tamanio_bloque);

    log_info(storage->logger, "Truncando file %s tag %s de %d a %d bytes", nombre_file, tag, tamanio_actual, nuevo_tamanio);

    if(bloques_nuevos > bloques_actuales) { // Aumentar tamaño
        for(int i = bloques_actuales; i < bloques_nuevos; i++) {
            char* path_logico = string_from_format("%s/files/%s/%s/logical_blocks/block%06d.dat", storage->punto_montaje, nombre_file, tag, i);
            char* path_fisico = obtener_ruta_absoluta("%s/physical_blocks/block0000.dat", storage->punto_montaje); 

            if(link(path_fisico, path_logico) != 0) {
                log_error(storage->logger, "Error al crear link para el %s -> %s", path_logico, path_fisico);
                free(path_logico);
                free(path_fisico);
                config_destroy(metadata_config);
                free(ruta_tag);
                free(ruta_metadata);
                return false;
            }

            log_info(storage->logger, "Bloque logico %d creado y linkeado con el bloque fisico 0", i);
            free(path_logico);
            free(path_fisico);
        }
    }

    else if(bloques_nuevos < bloques_actuales) { // Disminuir tamaño
        for(int i = bloques_nuevos - 1; i >= bloques_nuevos; i--) {
            char* path_logico = string_from_format("%s/files/%s/%s/logical_blocks/block%06d.dat", storage->punto_montaje, nombre_file, tag, i);

            if(access(path_logico, F_OK) == 0) {
                char path_fisico[512];
                ssize_t len = readlink(path_logico, path_fisico, sizeof(path_fisico) - 1);
                if (len != -1) {
                    path_fisico[len] = '\0'; // esto para usar path_fisico como string y usar stat

                    remove(path_logico); // Eliminar link logico

                    struct stat stat_buf;
                    if (stat(path_fisico, &stat_buf) == 0) { // checkea los stats del path fisico, en este caso nos interesa las cantidades de links
                        if (stat_buf.st_nlink == 1) {


                }
                log_info(storage->logger, "Bloque logico %d eliminado", i);
            }
        }
    }
}
    }




}

// probar si el path es correcto en la que se esta pasando con varios logs 

// o usar string_strim por si hay un salto de linea o espacio al final

// falta crear logical blocks 
