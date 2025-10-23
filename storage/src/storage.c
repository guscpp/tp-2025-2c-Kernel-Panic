#include "storage.h"

char* PATH_BASE;

t_storage* iniciar_storage(){
    t_storage* storage = malloc(sizeof(t_storage));

    storage->logger = iniciar_logger("storage.log", "STORAGE", 1, LOG_LEVEL_INFO);

    storage->config = iniciar_config(storage->logger, "storage.config");
    storage->superblock = iniciar_config(storage->logger, "superblock.config");

    storage->puerto_escucha = config_get_string_value(storage->config, "PUERTO_ESCUCHA");
    storage->fresh_start = config_get_int_value(storage->config, "FRESH_START");
    storage->punto_montaje = config_get_string_value(storage->config, "PUNTO_MONTAJE");
    storage->retardo_operacion = config_get_int_value(storage->config, "RETARDO_OPERACION");
    storage->retardo_acceso_bloque = config_get_int_value(storage->config, "RETARDO_ACCESO_BLOQUE");
    storage->log_level = config_get_string_value(storage->config, "LOG_LEVEL");
    storage->tamanio_bloque = config_get_int_value(storage->superblock, "BLOCK_SIZE");
    storage->tamanio_filesystem = config_get_int_value(storage->superblock, "FS_SIZE");

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
        perror("No se pudo abrir el archivo");
    }
    free(ruta_abs);
}

void crear_directorios(char* ruta_rel){
    char* ruta_abs = obtener_ruta_absoluta(ruta_rel);
    mkdir(ruta_abs, 0777);
    free(ruta_abs);
}


void recrear_bmap(int cantidad_bloques, char* path_bmap){
    FILE* archivo_bmap = fopen(path_bmap, "w");
    if(archivo_bmap != NULL){   
        int bytes = cantidad_bloques / 8;
        void* bitmap_data = calloc(1, bytes); // calloc inicializa alocando memoria en 0, apunta al principio del array de memoria
        fwrite(bitmap_data, 1, bytes, archivo_bmap);
        fclose(archivo_bmap);
        free(bitmap_data);
    }
    free(path_bmap);
}

void recrear_hash(char* path_hash){
    FILE* archivo_hash = fopen(path_hash, "w");
    if(archivo_hash != NULL){
        fclose(archivo_hash);
    }
    free(path_hash);

}

void crear_initial_file(t_storage* storage){
    crear_directorios("files/initial_file");
    crear_directorios("files/initial_file/BASE");
    crear_directorios("files/initial_file/logical_blocks");

    char* path_block0 = obtener_ruta_absoluta("physical_blocks/block0000.dat");
    FILE* block0 = fopen(path_block0, "w");
    if (block0){
        int block_size = config_get_int_value(storage->superblock, "BLOCK_SIZE");
        char* ceros = calloc(1, block_size); // pq char*? pero porque char ? y no void*?
        memset(ceros, 0, block_size);
        fwrite(ceros, 1, block_size, block0);
        fclose(block0);
        free(ceros);
    }else{
        log_error(storage->logger, "No se pudo crear el bloque inicial");
    }

    char* path_logico0 = obtener_ruta_absoluta("files/initial_file/logical_blocks/block000000.dat");
    if(link(path_block0, path_logico0) == 0){
        log_info(storage->logger, "Se creo el link del bloque inicial con el bloque fisico correctamente");
    }else{
        log_error(storage->logger, "No se pudo crear el link del bloque inicial con el bloque fisico");
    }


    char* metadata = obtener_ruta_absoluta("files/initial_file/BASE/metadata.config");
    FILE* archivo_metadata = fopen(metadata, "w");
    if(archivo_metadata){
        fprintf(archivo_metadata, "TAMANIO=%d\n", config_get_int_value(storage->superblock, "BLOCK_SIZE"));
        fprintf(archivo_metadata, "ESTADO=WORK_IN_PROGRESS\n");
        fprintf(archivo_metadata, "BLOQUES=[0]\n");
        fclose(archivo_metadata);
    }

    free(path_block0);
    free(path_logico0);
    free(metadata);
}

void formatear_fs(t_storage* storage){
    char* path_hash = obtener_ruta_absoluta("blocks_hash_index.config");
    char* path_bmap = obtener_ruta_absoluta("bitmap.bit");
    char* path_files = obtener_ruta_absoluta("files");
    char* path_phblck = obtener_ruta_absoluta("physical_blocks");

    rm_rf(path_hash);
    rm_rf(path_bmap);
    rm_rf(path_files);
    rm_rf(path_phblck);

    int tamanio_storage = config_get_int_value(storage->superblock, "FS_SIZE");
    int tamanio_bloque = config_get_int_value(storage->superblock, "BLOCK_SIZE");
    int cantidad_bloques = tamanio_storage / tamanio_bloque;

    recrear_bmap(cantidad_bloques, path_bmap);

    recrear_hash(path_hash);

    crear_directorios("files");
    free(path_files);
    crear_directorios("physical_blocks");
    free(path_phblck);


    crear_initial_file(storage);

}



bool inicializar_file_system(t_storage* storage){
    log_info(storage->logger, "Inicializando File System...");

    PATH_BASE = config_get_string_value(storage->config, "PUNTO_MONTAJE");

    if(storage->fresh_start){
        log_info(storage->logger, "FRESH_START=TRUE → Formateo inicial");
        formatear_fs(storage);
    }else{
        log_info(storage->logger, "FRESH_START=FALSE → Mantiene el contenido preexistente");
    }
    return true;
}

void enviar_tamanio_paquete_aworker(int worker_fd, t_storage* storage)
{
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(STORAGE_SEND_BLOCK_SIZE, buffer);
    //int tamanio_paquete = config_get_int_value(storage->config, "BLOCK_SIZE");
    int tamanio_paquete = storage->tamanio_bloque; //solo para probar que mande la respuesta
    agregar_a_paquete(paquete, &tamanio_paquete, sizeof(int));
    enviar_paquete(paquete, worker_fd, storage->logger);
    log_info(storage->logger, "Llegue a enviar");
}

