#include "storage.h"
#include <commons/collections/dictionary.h>


char* PATH_BASE = NULL;


// ****************************************************************************
t_storage* iniciar_storage(){
    t_storage* storage = malloc(sizeof(t_storage));

    storage->logger = iniciar_logger("storage.log", "STORAGE", 1, LOG_LEVEL_INFO);
    storage->config = iniciar_config(storage->logger, "storage.config");
    storage->superblock = iniciar_config(storage->logger, "superblock.config");
    storage->puerto_escucha = config_get_string_value(storage->config, "PUERTO_ESCUCHA");
    char* fresh_start_str = config_get_string_value(storage->config, "FRESH_START");
    storage->fresh_start = (strcmp(fresh_start_str, "TRUE") == 0) ? 1 : 0;
    storage->punto_montaje = config_get_string_value(storage->config, "PUNTO_MONTAJE");
    storage->retardo_operacion = config_get_int_value(storage->config, "RETARDO_OPERACION");
    storage->retardo_acceso_bloque = config_get_int_value(storage->config, "RETARDO_ACCESO_BLOQUE");
    storage->log_level = config_get_string_value(storage->config, "LOG_LEVEL");
    storage->tamanio_bloque = config_get_int_value(storage->superblock, "BLOCK_SIZE");
    storage->tamanio_filesystem = config_get_int_value(storage->superblock, "FS_SIZE");
    storage->bitmap = NULL;
    storage->path_bitmap = string_from_format("%s/bitmap.bin", storage->punto_montaje);

    pthread_mutex_init(&storage->mutex_bitmap, NULL);
    pthread_mutex_init(&storage->mutex_hash_index, NULL); 
    storage->dict_locks_files = dictionary_create();
    pthread_mutex_init(&storage->mutex_dict_locks, NULL);
    
    PATH_BASE = storage->punto_montaje; //se inicializa la variable global del Papu :p
    log_debug(storage->logger, "El storage se inicializo correctamente");
    return storage;
}


// ****************************************************************************
void verificar_storage(t_storage* s)
{
    log_debug(s->logger, "Puerto leido: %s", s->puerto_escucha);
    log_debug(s->logger, "Fresh leido: %d", s->fresh_start);
    log_debug(s->logger, "Punto de montaje leido: %s", s->punto_montaje);
    log_debug(s->logger, "Retardo operacion leido: %d", s->retardo_operacion);
    log_debug(s->logger, "Retardo acceso bloque leido: %d", s->retardo_acceso_bloque);
    log_debug(s->logger, "log level leido: %s", s->log_level);
}


// ****************************************************************************
void destruir_storage(t_storage* storage){
    if(storage != NULL){
        if(storage->bitmap != NULL){
            bitarray_destroy(storage->bitmap);
        }
        pthread_mutex_destroy(&storage->mutex_bitmap);
        dictionary_iterator(storage->dict_locks_files, destruir_mutex);
        dictionary_destroy(storage->dict_locks_files);
        
        pthread_mutex_destroy(&storage->mutex_dict_locks);
        log_destroy(storage->logger);
        config_destroy(storage->config);
        free(storage->path_bitmap);
        free(storage);
    }
}

int rm_rf (const char* path){
    char* comando = string_from_format("rm -rf %s", path);
    int aux = system(comando);
    free(comando);
    return aux;
}


// ****************************************************************************
char* obtener_ruta_absoluta(char* ruta_rel){
    char* aux = string_new();
    string_append(&aux, PATH_BASE);
    string_append(&aux, "/");
    string_append(&aux, ruta_rel);
    return aux;
}


// ****************************************************************************
void crear_archivos(char* ruta, char* modo) {
    char* ruta_abs = obtener_ruta_absoluta(ruta);
    FILE* archivo = fopen(ruta_abs, modo);
    if (archivo == NULL) {
        perror("No se pudo abrir el archivo");
    } else {
        fclose(archivo);
    }
    free(ruta_abs);
}

// ****************************************************************************
// Dada una ruta absoluta va creando cada dir individualmente
// Funcion pensada para usarse dentro de void crear_directorios(char* ruta_rel)
void mkdir_recursivo(const char* ruta_abs) {
    char* ruta = strdup(ruta_abs);
    if (!ruta) return;

    char* p = ruta;
    if (*p == '/') p++; // Saltar raíz

    for (int i=0; *p; p++, i++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(ruta, 0777) != 0 && errno != EEXIST) {
                printf("mkdir fallo el paso: %d", i);
            }
            *p = '/';
        }
    }
    if (mkdir(ruta, 0777) != 0 && errno != EEXIST) {
        perror("mkdir falló en último nivel");
    }
    free(ruta);
}


// ****************************************************************************
void crear_directorios(char* ruta_rel) {
    char* ruta_abs = obtener_ruta_absoluta(ruta_rel);
    mkdir_recursivo(ruta_abs);
    free(ruta_abs);
}


// ****************************************************************************
void recrear_bmap(t_storage* storage, int cantidad_bloques, char* path_bmap) {
    
    pthread_mutex_lock(&storage->mutex_bitmap);
    // Si existía un bitmap anterior lo destruimos
    if (storage->bitmap != NULL) {
        //free(storage->bitmap->bitarray); //libera el ya existente
        bitarray_destroy(storage->bitmap);
        storage->bitmap = NULL;
    }

    int bytes_necesarios = (cantidad_bloques / 8) + ((cantidad_bloques % 8 != 0) ? 1 : 0);
    void* bitmap_data = calloc(bytes_necesarios, 1); // Inicializado en 0

    if (bitmap_data == NULL) {
        log_error(storage->logger, "No se pudo allocar memoria para el bitmap en recrear_bmap");
        pthread_mutex_unlock(&storage->mutex_bitmap);
        return; //
    }

    // Crear el nuevo bitmap
    storage->bitmap = bitarray_create_with_mode(bitmap_data, bytes_necesarios, LSB_FIRST);

    if (storage->bitmap == NULL) {
        log_error(storage->logger, "No se pudo crear la estructura t_bitarray en recrear_bmap");
        free(bitmap_data);
        pthread_mutex_unlock(&storage->mutex_bitmap);
        return;
    }

    // Marcar el bloque 0 como ocupado (por initial_file)
    bitarray_set_bit(storage->bitmap, 0);

    // Guardar el bitmap en disco
    FILE* f = fopen(path_bmap, "wb");
    if (f) {
        fwrite(bitmap_data, 1, bytes_necesarios, f);
        fclose(f);
    } else {
        log_error(storage->logger, "No se pudo abrir %s para persistir bitmap", path_bmap);
    }

    pthread_mutex_unlock(&storage->mutex_bitmap);

}


// ****************************************************************************
void recrear_hash(char* path_hash){
    FILE* archivo_hash = fopen(path_hash, "w");
    if(archivo_hash != NULL){
        fclose(archivo_hash);
    }
    //free(path_hash);  //libera quien llama a recrear_hash()
}


// ****************************************************************************
void crear_initial_file(t_storage* storage){
    crear_directorios("files/initial_file");
    crear_directorios("files/initial_file/BASE");
    crear_directorios("files/initial_file/BASE/logical_blocks");

    char* path_block0 = obtener_ruta_absoluta("physical_blocks/block0000.dat");
    FILE* block0 = fopen(path_block0, "w");
    if (block0){
        int tamanio_bloque = storage->tamanio_bloque;
        void* ceros = calloc(1, tamanio_bloque); 
        memset(ceros, '0', tamanio_bloque);
        fwrite(ceros, 1, tamanio_bloque, block0);
        fclose(block0);
        free(ceros);
    }else{
        log_error(storage->logger, "No se pudo crear el bloque inicial");
        free(path_block0);
        return;
    }

    char* path_logico0 = obtener_ruta_absoluta("files/initial_file/BASE/logical_blocks/000000.dat");
    if(link(path_block0, path_logico0) == 0){
        log_debug(storage->logger, "Se creo el link del bloque inicial con el bloque fisico correctamente");
    }else{
        log_error(storage->logger, "No se pudo crear el link del bloque inicial con el bloque fisico");
    }


    char* metadata = obtener_ruta_absoluta("files/initial_file/BASE/metadata.config");
    FILE* archivo_metadata = fopen(metadata, "w");
    if(archivo_metadata){
        fprintf(archivo_metadata, "TAMANIO=0\n");
        fprintf(archivo_metadata, "ESTADO=COMMITED\n");
        fprintf(archivo_metadata, "BLOCKS=[0]\n");
        fclose(archivo_metadata);
    }

    free(path_block0);
    free(path_logico0);
    free(metadata);
}


// ****************************************************************************
void formatear_fs(t_storage* storage){
    char* path_hash = obtener_ruta_absoluta("blocks_hash_index.config");
    char* path_bmap = obtener_ruta_absoluta("bitmap.bin");
    char* path_files = obtener_ruta_absoluta("files");
    char* path_phblck = obtener_ruta_absoluta("physical_blocks");

    rm_rf(path_hash);
    rm_rf(path_bmap);
    rm_rf(path_files);
    rm_rf(path_phblck);

    int tamanio_filesystem = storage->tamanio_filesystem;
    int tamanio_bloque = storage->tamanio_bloque;
    int cantidad_bloques = tamanio_filesystem / tamanio_bloque;

    recrear_bmap(storage, cantidad_bloques, path_bmap);

    recrear_hash(path_hash);

    crear_directorios("files");
    crear_directorios("physical_blocks");

    // Crear TODOS los bloques físicos
    for (int i = 0; i < cantidad_bloques; i++) {
        char* bloque_path = string_from_format("physical_blocks/block%04d.dat", i);
        char* bloque_abs = obtener_ruta_absoluta(bloque_path);
        
        FILE* bloque_file = fopen(bloque_abs, "w");
        if (bloque_file) {
            // Inicializar con ceros
            void* ceros = calloc(1, storage->tamanio_bloque);
            fwrite(ceros, 1, storage->tamanio_bloque, bloque_file);
            free(ceros);
            fclose(bloque_file);
        }
        
        free(bloque_path);
        free(bloque_abs);
    }

    free(path_hash);
    free(path_bmap);
    free(path_files);
    free(path_phblck);

    crear_initial_file(storage);

}


// ****************************************************************************
bool inicializar_file_system(t_storage* storage){
    log_debug(storage->logger, "Inicializando File System...");

    PATH_BASE = storage->punto_montaje;
    char* path_bmap = storage->path_bitmap;

    if(storage->fresh_start){
        log_debug(storage->logger, "FRESH_START=TRUE → Formateo inicial");
        formatear_fs(storage);
    }else{
        log_debug(storage->logger, "FRESH_START=FALSE → Mantiene el contenido preexistente");

        FILE* f = fopen(path_bmap, "rb");
        if (!f) {
            log_error(storage->logger, "No existe bitmap.bin aunque FRESH_START=FALSE");
            return true;
        }

        // Obtener tamaño real del archivo
        fseek(f, 0, SEEK_END);
        size_t bitmap_size = ftell(f);
        rewind(f);

        // Calcular tamaño esperado
        int cantidad_bloques = storage->tamanio_filesystem / storage->tamanio_bloque;
        size_t expected_size = (cantidad_bloques + 7) / 8;

        if (bitmap_size != expected_size) {
            log_error(storage->logger, "Tamaño de bitmap.bin corrupto (%zu vs esperado %zu)", 
                      bitmap_size, expected_size);
            fclose(f);
            return true;
        }

        // Leer los bytes crudos
        char* bitarray_data = malloc(bitmap_size);
        if (fread(bitarray_data, 1, bitmap_size, f) != bitmap_size) {
            log_error(storage->logger, "Error al leer bitmap.bin");
            free(bitarray_data);
            fclose(f);
            return true;
        }
        fclose(f);

        // Crear el t_bitarray (commons bitarray_create toma ownership del puntero)
    storage->bitmap = bitarray_create_with_mode(bitarray_data, bitmap_size, LSB_FIRST);
    
        log_debug(storage->logger, "Bitmap cargado correctamente (%zu bytes, %d bloques)", 
                 bitmap_size, cantidad_bloques);
    }
    return true;
}


// ****************************************************************************
void enviar_tamanio_paquete_aworker(t_storage* storage, int worker_fd)
{
    t_buffer* buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(STORAGE_SEND_BLOCK_SIZE, buffer);
    //int tamanio_paquete = config_get_int_value(storage->config, "BLOCK_SIZE");
    int tamanio_paquete = storage->tamanio_bloque; //solo para probar que mande la respuesta
    agregar_a_paquete(paquete, &tamanio_paquete, sizeof(int));
    enviar_paquete(paquete, worker_fd, storage->logger);
    log_debug(storage->logger, "Llegue a enviar");
}


// ****************************************************************************
// Función helper para destruir cada mutex dentro del diccionario
void destruir_mutex_lock(void* data) {
    pthread_mutex_t* mutex = (pthread_mutex_t*) data;
    pthread_mutex_destroy(mutex);
    free(mutex);
}


// ****************************************************************************
// Liberar cada mutex individualmente antes de destruir el diccionario
void destruir_mutex(char* key, void* value) {
    pthread_mutex_t* mutex = (pthread_mutex_t*)value;
    pthread_mutex_destroy(mutex);
    free(mutex);
}


// ****************************************************************************
void destruir_dict_locks(t_dictionary* dict) {
    if (!dict) return;
    dictionary_iterator(dict, destruir_mutex);
    dictionary_destroy(dict);
}


bool escribir_bloque_test(t_storage* storage,  char* file, char* tag,int bloque_logico, char* contenido) { // test
    // Simulamos los parámetros que vendrían del Worker
    t_list* parametros = list_create();

    int op = STORAGE_WRITE_BLOCK;   // dummy
    int query_id = 999;             // id ficticio para el log
    int bloque_copia = bloque_logico;  // copia porque list_add guarda puntero
    char* file_dup = string_duplicate(file);
    char* tag_dup  = string_duplicate(tag);
    char* cont_dup = string_duplicate(contenido);

    // Indices según tu función:
    // 0: cod_op, 1: query_id, 2:file, 3:tag, 4:bloque_logico, 5:contenido
    list_add(parametros, &op);
    list_add(parametros, &query_id);
    list_add(parametros, file_dup);
    list_add(parametros, tag_dup);
    list_add(parametros, &bloque_copia);
    list_add(parametros, cont_dup);

    bool ok = escribir_bloque(storage, parametros);

    list_destroy(parametros);
    free(file_dup);
    free(tag_dup);
    free(cont_dup);

    return ok;
}