#include "storage.h"

bool crear_file(t_storage* storage, t_list* parametros)
{
    if (!parametros || list_size(parametros) < 4) {
        log_error(storage->logger, "Parámetros insuficientes para STORAGE_CREATE_FILE");
        return false;
    }

    int query_id = *(int*)list_get(parametros, 1);
    char* nombre_file = list_get(parametros, 2);
    char* tag_inicial = list_get(parametros, 3);

    //mini-validacion
    if (!nombre_file || !tag_inicial || strlen(nombre_file) == 0 || strlen(tag_inicial) == 0) {
        log_error(storage->logger, "Nombre de File o Tag inválido");
        return false;
    }

    //construir rutas RELATIVAS
    char* ruta_rel_tag = string_from_format("files/%s/%s", nombre_file, tag_inicial);
    char* ruta_abs_tag = string_from_format("%s/%s", storage->punto_montaje, ruta_rel_tag);

    //verificar si ya existe el File y/o el Tag
    if (access(ruta_abs_tag, F_OK) == 0) {
        log_error(storage->logger, "File %s o Tag %s ya existe", nombre_file, tag_inicial);
        free(ruta_rel_tag);
        free(ruta_abs_tag);
        return false;
    }

    //crear el directorio del tag
    crear_directorios(ruta_rel_tag);
    free(ruta_rel_tag);

    //crear el subdirectorio logical_blocks
    char* ruta_rel_logical = string_from_format("files/%s/%s/logical_blocks", nombre_file, tag_inicial);
    crear_directorios(ruta_rel_logical);
    free(ruta_rel_logical);

    // Crear metadata.config
    char* metadata_path = string_from_format("%s/metadata.config", ruta_abs_tag);
    FILE* metadata_file = fopen(metadata_path, "w");
    if (!metadata_file) {
        log_error(storage->logger, "Error al crear metadata.config para %s:%s", nombre_file, tag_inicial);
        free(ruta_abs_tag);
        free(metadata_path);
        return false;
    }

    fprintf(metadata_file, "TAMANIO=0\n");
    fprintf(metadata_file, "ESTADO=WORK_IN_PROGRESS\n");
    fprintf(metadata_file, "BLOCKS=[]\n");  // ← CORRECTO: "BLOCKS", NO "BLOQUES"
    fclose(metadata_file);
    free(metadata_path);

    usleep(storage->retardo_operacion * 1000);  //retardo olbigatorio

    // Log obligatorio con QUERY_ID
    log_info(storage->logger, "##%d- File Creado %s:%s", query_id, nombre_file, tag_inicial);

    free(ruta_abs_tag);
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
            char* path_fisico = string_from_format("%s/physical_blocks/block0000.dat", storage->punto_montaje); 
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
        for(int i = bloques_nuevos - 1; i >= bloques_nuevos; i--){
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
                            marcar_bloque_libre(storage, path_fisico);
                            log_info(storage->logger, "Bloque fisico %s liberado", path_fisico);
                        }

                    }
                }
            }
            free(path_logico);
        }
    }

    // falta actualizar metada y implementar marcar_bloque_libre
}



// probar si el path es correcto en la que se esta pasando con varios logs 

// o usar string_strim por si hay un salto de linea o espacio al final

// falta crear logical blocks 
