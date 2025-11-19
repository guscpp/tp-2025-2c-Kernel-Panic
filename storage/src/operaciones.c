#include "storage.h"
#include "operaciones.h"
#include <sys/stat.h>       // Para stat()
#include <commons/string.h> // Para string_*, get_array_length
#include <commons/collections/dictionary.h>


// ****************************************************************************
// Obtener la longitud de un array de strings
int get_array_length(char** array) {
    if (!array) return 0;
    int length = 0;
    while (array[length] != NULL) {
        length++;
    }
    return length;
}


// ****************************************************************************
void marcar_bloque_libre(t_storage* storage, int query_id, int numero_bloque) {
    int cantidad_bloques = storage->tamanio_filesystem / storage->tamanio_bloque;

    if (numero_bloque < 0 || numero_bloque >= cantidad_bloques) {
        log_warning(storage->logger, "Intento de liberar bloque inválido: %d", numero_bloque);
        return;
    }

    pthread_mutex_lock(&storage->mutex_bitmap);

    if (!bitarray_test_bit(storage->bitmap, numero_bloque)) {
        log_warning(storage->logger, "Bloque %d ya estaba libre", numero_bloque);

        //se desbloquea antes de retornar, y tambien fuera del if
        pthread_mutex_unlock(&storage->mutex_bitmap);

        return;
    }

    bitarray_clean_bit(storage->bitmap, numero_bloque);
    pthread_mutex_unlock(&storage->mutex_bitmap);
    log_info(storage->logger, "Bloque físico %d marcado como libre", numero_bloque);
}


// ****************************************************************************
bool crear_file(t_storage* storage, t_list* parametros)
{
    if (!parametros || list_size(parametros) < 4) {
        log_error(storage->logger, "Parámetros insuficientes para STORAGE_CREATE_FILE");
        return false;
    }

    int query_id = *(int*)list_get(parametros, 1);
    char* nombre_file = list_get(parametros, 2);
    char* tag_inicial = list_get(parametros, 3);

    // Obtener lock en el diccionario
    pthread_mutex_t* file_mutex = get_or_create_file_mutex(storage, nombre_file, tag_inicial);
    pthread_mutex_lock(file_mutex);

    //mini-validacion
    if (!nombre_file || !tag_inicial || strlen(nombre_file) == 0 || strlen(tag_inicial) == 0) {
        log_error(storage->logger, "Nombre de File o Tag inválido");
        pthread_mutex_unlock(file_mutex);
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
        pthread_mutex_unlock(file_mutex);
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
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    fprintf(metadata_file, "TAMANIO=0\n");
    fprintf(metadata_file, "ESTADO=WORK_IN_PROGRESS\n");
    fprintf(metadata_file, "BLOCKS=[]\n");  // <-- CORRECTO: "BLOCKS", NO "BLOQUES"
    fclose(metadata_file);
    free(metadata_path);

    usleep(storage->retardo_operacion * 10);  //retardo olbigatorio

    // Log obligatorio con QUERY_ID
    log_info(storage->logger, "##%d- File Creado %s:%s", query_id, nombre_file, tag_inicial);

    free(ruta_abs_tag);
    pthread_mutex_unlock(file_mutex);
    return true;
}


// ****************************************************************************
char* path_logico_para_truncate(const char* punto_montaje, const char* nombre_file, const char* tag, int i) {
    return string_from_format("%s/files/%s/%s/logical_blocks/%06d.dat",
                              punto_montaje, nombre_file, tag, i);
}


// ****************************************************************************
char* path_fisico_para_truncate(const char* punto_montaje, int bloque_fisico_id) {
    return string_from_format("%s/physical_blocks/block%04d.dat", punto_montaje, bloque_fisico_id);
}

// ****************************************************************************
int* leer_bloques_actuales(t_config* metadata_config, int* cantidad_bloques_fisico) {
    *cantidad_bloques_fisico = 0;
    char** array = config_get_array_value(metadata_config, "BLOCKS");
    if( !array ) {
        printf("Leer bloques actuales: BLOCKS no encontrado o vacío (NULL)");
        return NULL;
    }
    
    //verificar si la lista esta efectivamente vacia: BLOCKS=[]
    //caso 1: config_get_array_value devuelve ["", NULL] o [NULL] para BLOCKS=[]
    if (array[0] == NULL || (array[0] != NULL && strlen(array[0]) == 0 && array[1] == NULL)) {
         printf("Leer bloques actuales: BLOCKS=[] (lista vacía detectada como [\"\", NULL] o [NULL])");
         string_array_destroy(array);
         return NULL; // Debe devolver NULL para indicar 0 bloques
    }

    //caso 2: La lista tiene elementos validos
    int n = 0;
    while(array[n] != NULL && strlen(array[n]) > 0) { // Asegura que no sea string vacío
        n++;
    }
    if(n == 0) { //si el primer elemento es "" o hay solo strings vacios
        printf("Leer bloques actuales: Conteo de bloques es 0 (solo strings vacíos)");
        string_array_destroy(array);
        return NULL; //devolver NULL si no hay bloques válidos
    }
    int* array_bloques = malloc(sizeof(int) * n);
    if (!array_bloques) {
        printf("No se pudo allocar memoria para array_bloques en leer_bloques_actuales");
        string_array_destroy(array); //liberar el array original
        return NULL;
    }
    for(int i = 0; i < n; i++) {
        array_bloques[i] = atoi(array[i]);
    }
    *cantidad_bloques_fisico = n;
    string_array_destroy(array); //liberar el array de strings devuelto por config_get_array_value
    return array_bloques;
}


// ****************************************************************************
char* serializar_bloques(const int* bloques, int cantidad_bloques) {
    char* resultado = string_new();
    string_append(&resultado, "[");
    for(int i = 0; i < cantidad_bloques; i++) {
        char* num = string_from_format("%d", bloques[i]);
        string_append(&resultado, num);
        free(num);
        if(i < cantidad_bloques - 1) {
            string_append(&resultado, ",");
        }
    }
    string_append(&resultado, "]");
    return resultado;

}


// ****************************************************************************
bool truncar_file(t_storage* storage, t_list* parametros)
{
    if (!parametros || list_size(parametros) < 5) {
        log_error(storage->logger, "Parametros invalidos para truncar_file");
        return false;
    }
    int query_id = *(int*)list_get(parametros, 1);
    char* nombre_file = list_get(parametros, 2);
    char* tag = list_get(parametros, 3);
    int nuevo_tamanio = *(int*) list_get(parametros, 4);

    // Obtener lock en el diccionario
    pthread_mutex_t* file_mutex = get_or_create_file_mutex(storage, nombre_file, tag);
    pthread_mutex_lock(file_mutex);

    // La ruta correcta es .../files/nombre_file/tag/metadata.config
    char* ruta_metadata = string_from_format("%s/files/%s/%s/metadata.config", storage->punto_montaje, nombre_file, tag);

    if(access(ruta_metadata, F_OK) != 0) {
        log_error(storage->logger, "El tag %s del file %s no existe", tag, nombre_file);
        free(ruta_metadata);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    t_config* metadata_config = config_create(ruta_metadata);
    if(!metadata_config){
        log_error(storage->logger, "No se pudo abrir el metadata.config para truncar el file %s tag %s", nombre_file, tag);
        free(metadata_config);
        free(ruta_metadata);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    int tamanio_actual = config_get_int_value(metadata_config, "TAMANIO");
    int tamanio_bloque = storage->tamanio_bloque;
    
    // División entera hacia arriba para obtener bloques actuales/nuevos
    int bloques_actuales = (tamanio_actual + tamanio_bloque - 1) / tamanio_bloque;
    int bloques_nuevos = (nuevo_tamanio + tamanio_bloque - 1) / tamanio_bloque;

    log_info(storage->logger, "Truncando file %s tag %s de %d a %d bytes", nombre_file, tag, tamanio_actual, nuevo_tamanio);

    int cantidad_bloques_fisico = 0;
    int* array_bloques_fisico = leer_bloques_actuales(metadata_config, &cantidad_bloques_fisico);

    log_info(storage->logger, "Truncar %s:%s de %d→%d bytes (%d→%d bloques)",
             nombre_file, tag, tamanio_actual, nuevo_tamanio, bloques_actuales, bloques_nuevos);

    if(bloques_nuevos > bloques_actuales) { // Aumentar tamaño
        for(int i = bloques_actuales; i < bloques_nuevos; i++) {
            char* path_logico = path_logico_para_truncate(storage->punto_montaje, nombre_file, tag, i);
            char* path_fisico = path_fisico_para_truncate(storage->punto_montaje, 0); // Asigna bloque 0 temporalmente
            
            // Crear directorio lógico si no existe
            char* dir_logico = string_substring_until(path_logico, string_length(path_logico) - 10); // Quitar "blockXXXXXX.dat"
            mkdir_recursivo(dir_logico);
            free(dir_logico);

            if(link(path_fisico, path_logico) != 0) {
                log_error(storage->logger, "Error al crear link para el %s -> %s", path_logico, path_fisico);
                free(path_logico);
                free(path_fisico);
                config_destroy(metadata_config);
                free(ruta_metadata);
                if(array_bloques_fisico) free(array_bloques_fisico);
                pthread_mutex_unlock(file_mutex);
                return false;
            }
            log_info(storage->logger, "Bloque logico %d creado y linkeado con el bloque fisico 0", i);
            free(path_logico);
            free(path_fisico);
        }
    }

    // Disminuir tamaño
    if(bloques_nuevos < bloques_actuales) { 
        for(int i = bloques_actuales - 1; i >= bloques_nuevos; i--){
            char* path_logico = string_from_format("%s/files/%s/%s/logical_blocks/block%06d.dat", storage->punto_montaje, nombre_file, tag, i);
            int array_fisico_id = 0;
            if(array_bloques_fisico && i < cantidad_bloques_fisico) {
                array_fisico_id = array_bloques_fisico[i];
            }
            if(access(path_logico, F_OK) == 0) {
                if(unlink(path_logico) != 0) {
                    log_error(storage->logger, "Error al eliminar el bloque logico %s", path_logico);
                    free(path_logico);
                    config_destroy(metadata_config);
                    free(ruta_metadata);
                    if(array_bloques_fisico) free(array_bloques_fisico);
                    pthread_mutex_unlock(file_mutex);
                    return false;
                }
            }
            free(path_logico);
            char* path_fisico = path_fisico_para_truncate(storage->punto_montaje, array_fisico_id);
            struct stat st;
            if (stat(path_fisico, &st) == 0) {
                if(st.st_nlink == 1) { // Si es el ultimo link
                    marcar_bloque_libre(storage, query_id, array_fisico_id);
                    log_info(storage->logger, "Bloque fisico %d liberado", array_fisico_id);
                }
            }
            free(path_fisico);
        }
    }

    // Reconstruir la lista de bloques físicos para la nueva cantidad
    int cantidad_bloques_fisico_nueva = bloques_nuevos;
    int* bloque_fisico_final = NULL;
    if(cantidad_bloques_fisico_nueva > 0) {
        bloque_fisico_final = malloc(sizeof(int) * cantidad_bloques_fisico_nueva);
        if (!bloque_fisico_final) {
            log_error(storage->logger, "Error al allocar memoria para bloque_fisico_final en truncar_file");
            config_destroy(metadata_config);
            free(ruta_metadata);
            if(array_bloques_fisico) free(array_bloques_fisico);
            pthread_mutex_unlock(file_mutex);
            return false;
        }
        int copiar = (cantidad_bloques_fisico < cantidad_bloques_fisico_nueva ? cantidad_bloques_fisico : cantidad_bloques_fisico_nueva);
        
        // Verificar si array_bloques_fisico no es NULL antes de copiar
        if (array_bloques_fisico != NULL) {
            for(int i = 0; i < copiar; i++) {
                bloque_fisico_final[i] = array_bloques_fisico[i];
            }
        }
        for(int i = copiar; i < cantidad_bloques_fisico_nueva; i++) {
            bloque_fisico_final[i] = 0; // Nuevo bloque, apunta a 0 inicialmente
        }
    }

    // Actualizar metadata
    char* tamanio_final = string_from_format("%d", nuevo_tamanio);
    config_set_value(metadata_config, "TAMANIO", tamanio_final);
    free(tamanio_final);

    char* bloques_serializados = serializar_bloques(bloque_fisico_final, cantidad_bloques_fisico_nueva);
    config_set_value(metadata_config, "BLOCKS", bloques_serializados);
    config_save(metadata_config);
    free(bloques_serializados);

    if(bloque_fisico_final) free(bloque_fisico_final);
    if(array_bloques_fisico) free(array_bloques_fisico);

    config_destroy(metadata_config);
    free(ruta_metadata);
    pthread_mutex_unlock(file_mutex);
    return true;
}


// ****************************************************************************
// Crea un nuevo Tag como copia exacta de un Tag existente dentro del mismo File
// duplicando su metadata y hard links a bloques físicos.
bool tag_file(t_storage* storage, t_list* parametros){
    if(!parametros || list_size(parametros) < 5){
        log_error(storage->logger, "Parametros invalidos para tag_file");
        return false;
    }
    int query_id = *(int*)list_get(parametros, 1);
    char* nombre_file = list_get(parametros, 2);
    char* tag_origen = list_get(parametros, 3);
    char* tag_destino = list_get(parametros, 4);

    // Obtener lock en el diccionario, pero... (ver mas abajo)
    char* lock_origen = string_from_format("%s:%s", nombre_file, tag_origen);
    char* lock_destino = string_from_format("%s:%s", nombre_file, tag_destino);
    
    // Determinar orden alfabético
    pthread_mutex_t* mutex_a = NULL;
    pthread_mutex_t* mutex_b = NULL;
    char* key_a = NULL;
    char* key_b = NULL;
    
    if(strcmp(lock_origen, lock_destino) < 0) {
        // lock_origen viene primero alfabéticamente
        key_a = lock_origen;
        key_b = lock_destino;
        mutex_a = get_or_create_file_mutex(storage, nombre_file, tag_origen);
        mutex_b = get_or_create_file_mutex(storage, nombre_file, tag_destino);
    } else {
        // lock_destino viene primero alfabéticamente  
        key_a = lock_destino;
        key_b = lock_origen;
        mutex_a = get_or_create_file_mutex(storage, nombre_file, tag_destino);
        mutex_b = get_or_create_file_mutex(storage, nombre_file, tag_origen);
    }
    
    // Adquirir en orden alfabético: primero mutex_a, luego mutex_b
    pthread_mutex_lock(mutex_a);
    pthread_mutex_lock(mutex_b);

    free(lock_origen);
    free(lock_destino);

    // 1. Validar existencia del tag origen y no existencia del tag destino
    // Crear rutas temporales para obtener_ruta_absoluta
    char* temp_path_origen = string_from_format("files/%s/%s", nombre_file, tag_origen);
    char* temp_path_destino = string_from_format("files/%s/%s", nombre_file, tag_destino);

    char* ruta_tag_origen = obtener_ruta_absoluta(temp_path_origen);
    char* ruta_tag_destino = obtener_ruta_absoluta(temp_path_destino);

    // Liberar los temporales *después* de usarlos en obtener_ruta_absoluta
    free(temp_path_origen);
    free(temp_path_destino);

    if(access(ruta_tag_origen, F_OK) != 0){
        log_error(storage->logger, "El tag %s del file %s no existe", tag_origen, nombre_file);
        free(ruta_tag_origen);
        free(ruta_tag_destino);
        pthread_mutex_unlock(mutex_b);
        pthread_mutex_unlock(mutex_a);
        return false;
    }

    if(access(ruta_tag_destino, F_OK) == 0){
        log_error(storage->logger, "El tag destino %s del file %s ya existe", tag_destino, nombre_file);
        free(ruta_tag_origen);
        free(ruta_tag_destino);
        pthread_mutex_unlock(mutex_b);
        pthread_mutex_unlock(mutex_a);
        return false;
    }

    // 2. Crear directorios del tag destino
    char* temp_path_tag_destino = string_from_format("files/%s/%s", nombre_file, tag_destino);
    char* temp_path_logical_blocks = string_from_format("files/%s/%s/logical_blocks", nombre_file, tag_destino);
    crear_directorios(temp_path_tag_destino);
    crear_directorios(temp_path_logical_blocks);
    free(temp_path_tag_destino);
    free(temp_path_logical_blocks);

    // 3. Cargar metadata del tag origen
    char* ruta_metadata_origen = string_from_format("%s/metadata.config", ruta_tag_origen);
    t_config* metadata_origen = config_create(ruta_metadata_origen);
    if(!metadata_origen){
        log_error(storage->logger, "No se pudo abrir el metadata.config origen para tag_file %s:%s", nombre_file, tag_origen);
        free(metadata_origen);
        free(ruta_tag_origen);
        free(ruta_tag_destino);
        free(ruta_metadata_origen);
        pthread_mutex_unlock(mutex_b);
        pthread_mutex_unlock(mutex_a);
        return false;
    }

    // 4. Crear archivo de metadata vacío para el tag destino
    char* ruta_metadata_destino = string_from_format("%s/metadata.config", ruta_tag_destino);
    FILE* metadata_nuevo_file = fopen(ruta_metadata_destino, "w");
    if(!metadata_nuevo_file){
        log_error(storage->logger, "Error al crear metadata.config para %s:%s", nombre_file, tag_destino);
        config_destroy(metadata_origen);
        free(ruta_tag_origen);
        free(ruta_tag_destino);
        free(ruta_metadata_origen);
        free(ruta_metadata_destino);
        pthread_mutex_unlock(mutex_b);
        pthread_mutex_unlock(mutex_a);
        return false;
    }
    fclose(metadata_nuevo_file);

    // 5. Cargar metadata del tag destino recién creado
    t_config* metadata_destino = config_create(ruta_metadata_destino);
    if(!metadata_destino){
        log_error(storage->logger, "No se pudo abrir el metadata.config destino para tag_file %s:%s", nombre_file, tag_destino);
        config_destroy(metadata_origen);
        free(metadata_destino);
        free(ruta_tag_origen);
        free(ruta_tag_destino);
        free(ruta_metadata_origen);
        free(ruta_metadata_destino);
        pthread_mutex_unlock(mutex_b);
        pthread_mutex_unlock(mutex_a);
        return false;
    }

    // 6. Copiar valores de tamaño, bloques y estado (WORK_IN_PROGRESS)
    int tamanio_origen = config_get_int_value(metadata_origen, "TAMANIO");
    char* bloques_origen = string_duplicate(config_get_string_value(metadata_origen, "BLOCKS")); // Copia string, se libera
    char* tamanio_origen_str = string_itoa(tamanio_origen);


    config_set_value(metadata_destino, "TAMANIO", tamanio_origen_str);
    config_set_value(metadata_destino, "BLOCKS", bloques_origen);
    config_set_value(metadata_destino, "ESTADO", "WORK_IN_PROGRESS");

    config_save(metadata_destino);

    // 7. Liberar recursos de metadata
    config_destroy(metadata_origen); // Libera la estructura y sus strings internos
    config_destroy(metadata_destino); // Libera la estructura y sus strings internos
    free(bloques_origen); // Libera la copia hecha con string_duplicate
    free(tamanio_origen_str);

    // 8. Copiar hard links de bloques lógicos
    char* ruta_logical_origen = string_from_format("%s/logical_blocks", ruta_tag_origen);
    char* ruta_logical_destino = string_from_format("%s/logical_blocks", ruta_tag_destino);

    DIR* dir = opendir(ruta_logical_origen);
    if(dir){
        struct dirent* entry;
        while((entry = readdir(dir)) != NULL){
            if(entry->d_type == DT_REG){ // Solo archivos regulares
                char* bloque_origen = string_from_format("%s/%s", ruta_logical_origen, entry->d_name);
                char* bloque_destino = string_from_format("%s/%s", ruta_logical_destino, entry->d_name);

                if(link(bloque_origen, bloque_destino) == 0){
                    log_info(storage->logger, "Bloque lógico %s linkeado a %s", bloque_origen, bloque_destino);
                } else {
                    log_error(storage->logger, "Error al linkear bloque lógico %s a %s", bloque_origen, bloque_destino);
                    free(bloque_origen);
                    free(bloque_destino);
                    closedir(dir);

                    // Liberar recursos antes de retornar
                    free(ruta_tag_origen);
                    free(ruta_tag_destino);
                    free(ruta_metadata_origen);
                    free(ruta_metadata_destino);
                    free(ruta_logical_origen);
                    free(ruta_logical_destino);
                    pthread_mutex_unlock(mutex_b);
                    pthread_mutex_unlock(mutex_a);
                    return false;
                }
                free(bloque_origen);
                free(bloque_destino);
            }
        }
        closedir(dir);
    } else {
        log_error(storage->logger, "Error al abrir el directorio de bloques lógicos para tag_file %s:%s", nombre_file, tag_origen);

        // Liberar recursos antes de retornar
        free(ruta_tag_origen);
        free(ruta_tag_destino);
        free(ruta_metadata_origen);
        free(ruta_metadata_destino);
        free(ruta_logical_origen);
        free(ruta_logical_destino);
        pthread_mutex_unlock(mutex_b);
        pthread_mutex_unlock(mutex_a);
        return false;
    }

    // 9. Liberar recursos temporales
    free(ruta_tag_origen);
    free(ruta_tag_destino);
    free(ruta_metadata_origen);
    free(ruta_metadata_destino);
    free(ruta_logical_origen);
    free(ruta_logical_destino);

    log_info(storage->logger, "##<%d>- Tag creado <%s>:<%s>", query_id, nombre_file, tag_destino);

    pthread_mutex_unlock(mutex_b);
    pthread_mutex_unlock(mutex_a);

    return true;
}

// ****************************************************************************
bool leer_bloque(t_storage* storage, t_list* parametros, void** contenido, int* tamanio_bloque) {
    if (!parametros || list_size(parametros) < 5) {
        log_error(storage->logger, "Parámetros insuficientes para STORAGE_READ_BLOCK");
        return false;
    }
    int query_id = *(int*)list_get(parametros, 1);
    char* nombre_file = list_get(parametros, 2);
    char* tag = list_get(parametros, 3);
    int bloque_logico = *(int*)list_get(parametros, 4);

    // Obtener lock en el diccionario
    pthread_mutex_t* file_mutex = get_or_create_file_mutex(storage, nombre_file, tag);
    pthread_mutex_lock(file_mutex);

    if (!nombre_file || !tag || strlen(nombre_file) == 0 || strlen(tag) == 0) {
        log_error(storage->logger, "Nombre de File o Tag inválido");
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    char* ruta_tag = string_from_format("%s/files/%s/%s", storage->punto_montaje, nombre_file, tag);
    if (access(ruta_tag, F_OK) != 0) {
        log_error(storage->logger, "File:Tag inexistente: %s:%s", nombre_file, tag);
        free(ruta_tag);
        pthread_mutex_unlock(file_mutex);
        return false;
    }
    free(ruta_tag);

    char* metadata_path = string_from_format("%s/files/%s/%s/metadata.config",
                                            storage->punto_montaje, nombre_file, tag);
    t_config* metadata = config_create(metadata_path);
    if (!metadata) {
        log_error(storage->logger, "No se pudo cargar metadata de %s:%s", nombre_file, tag);
        free(metadata);
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    int tam_archivo = config_get_int_value(metadata, "TAMANIO");
    int bloques_logicos_totales = (tam_archivo + storage->tamanio_bloque - 1) / storage->tamanio_bloque;

    if (bloque_logico < 0 || bloque_logico >= bloques_logicos_totales) {
        log_error(storage->logger, "Lectura fuera de límite: bloque lógico %d en archivo de %d bytes",
                  bloque_logico, tam_archivo);
        config_destroy(metadata);
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    config_destroy(metadata);
    free(metadata_path);

    char* nombre_bloque = string_from_format("%06d.dat", bloque_logico);
    char* ruta_bloque_logico = string_from_format("%s/files/%s/%s/logical_blocks/%s",
                                                 storage->punto_montaje, nombre_file, tag, nombre_bloque);
    free(nombre_bloque);

    FILE* f_bloque = fopen(ruta_bloque_logico, "r"); //deberia ser "rb" si son binarios
    if (!f_bloque) {
        log_error(storage->logger, "No se encontró el bloque lógico: %s", ruta_bloque_logico);
        free(ruta_bloque_logico);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    *contenido = malloc(storage->tamanio_bloque);
    if (!*contenido) {
        log_error(storage->logger, "Error al allocar memoria para contenido de bloque en leer_bloque");
        fclose(f_bloque);
        free(ruta_bloque_logico);
        pthread_mutex_unlock(file_mutex);
        return false;
    }
    size_t leido = fread(*contenido, 1, storage->tamanio_bloque, f_bloque);
    fclose(f_bloque);
    free(ruta_bloque_logico);

    if (leido < storage->tamanio_bloque) {
        //rellenar con ceros si el bloque leído es más pequeño que el tamaño de bloque
        memset((char*)(*contenido) + leido, 0, storage->tamanio_bloque - leido);
    }
    *tamanio_bloque = storage->tamanio_bloque;

    //aplicar retardos
    usleep(storage->retardo_operacion * 10);
    usleep(storage->retardo_acceso_bloque * 10);

    //log obligatorio
    log_info(storage->logger, "##%d- Bloque Lógico Leído %s:%s - Número de Bloque: %d",
             query_id, nombre_file, tag, bloque_logico);
    pthread_mutex_unlock(file_mutex);
    return true;
}

//*****************************************************************************
bool escribir_bloque(t_storage* storage, t_list* parametros) {
    if (!parametros || list_size(parametros) < 6) { 
        // 0:cod_op, 1:query_id, 2:file, 3:tag, 4:bloque_logico, 5:contenido
        log_error(storage->logger, "Parámetros insuficientes para STORAGE_WRITE_BLOCK");
        return false;
    }

    int query_id = *(int*)list_get(parametros, 1);
    char* nombre_file = list_get(parametros, 2);
    char* tag = list_get(parametros, 3);
    int bloque_logico = *(int*)list_get(parametros, 4);
    char* contenido = list_get(parametros, 5); //decidir en Worker si el contenido es un string
    int tamanio_contenido = strlen(contenido);

    // Obtener lock en el diccionario
    pthread_mutex_t* file_mutex = get_or_create_file_mutex(storage, nombre_file, tag);
    pthread_mutex_lock(file_mutex);

    if (!nombre_file || !tag || strlen(nombre_file) == 0 || strlen(tag) == 0) {
        log_error(storage->logger, "Nombre de File o Tag inválido");
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // 1. Verificar existencia del File:Tag
    char* ruta_tag = string_from_format("%s/files/%s/%s", storage->punto_montaje, nombre_file, tag);
    if (access(ruta_tag, F_OK) != 0) {
        log_error(storage->logger, "Intento de escritura en File:Tag inexistente: %s:%s", nombre_file, tag);
        free(ruta_tag);
        pthread_mutex_unlock(file_mutex);
        return false;
    }
    free(ruta_tag);

    // 2. Cargar metadata
    char* metadata_path = string_from_format("%s/files/%s/%s/metadata.config",
                                            storage->punto_montaje, nombre_file, tag);
    t_config* metadata = config_create(metadata_path);
    if (!metadata) {
        log_error(storage->logger, "No se pudo cargar metadata de %s:%s", nombre_file, tag);
        free(metadata);
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    char* estado_str = config_get_string_value(metadata, "ESTADO");
    if (strcmp(estado_str, "COMMITED") == 0) {
        log_error(storage->logger, "Intento de escritura en File:Tag COMMITED: %s:%s", nombre_file, tag);
        config_destroy(metadata);
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    int tam_archivo = config_get_int_value(metadata, "TAMANIO");
    int bloques_logicos_totales = (tam_archivo + storage->tamanio_bloque - 1) / storage->tamanio_bloque;

    if (bloque_logico < 0 || bloque_logico >= bloques_logicos_totales) {
        log_error(storage->logger, "Escritura fuera de límite: bloque lógico %d en archivo de %d bytes",
                  bloque_logico, tam_archivo);
        config_destroy(metadata);
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // Obtener lista de bloques fisicos actuales
    char** bloques_fisicos_array = config_get_array_value(metadata, "BLOCKS");
    if (!bloques_fisicos_array) { 
        // Verificar si la lista está vacia ~~o es NULL~~
        log_error(storage->logger, "Metadata de %s:%s no tiene bloques físicos asignados", nombre_file, tag);
        config_destroy(metadata);
        if (bloques_fisicos_array) string_array_destroy(bloques_fisicos_array); //liberar el array de strings
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // Verificar que el bloque_logico este dentro del rango del array de bloques
    int cantidad_bloques_metadata = get_array_length(bloques_fisicos_array);
    if (bloque_logico >= cantidad_bloques_metadata) {
        log_error(storage->logger, "Índice de bloque lógico %d fuera de rango para la lista de bloques físicos (longitud: %d)", bloque_logico, cantidad_bloques_metadata);
        config_destroy(metadata);
        if (bloques_fisicos_array) {
            for (int i = 0; bloques_fisicos_array[i] != NULL; i++) free(bloques_fisicos_array[i]);
            free(bloques_fisicos_array);
        }
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // Verificar tambien que el string no sea NULL o vacío
    if (bloques_fisicos_array[bloque_logico] == NULL || strlen(bloques_fisicos_array[bloque_logico]) == 0) {
        log_error(storage->logger, "Bloque físico en posición %d es inválido", bloque_logico);
        config_destroy(metadata);
        for (int i = 0; bloques_fisicos_array[i] != NULL; i++) {
            free(bloques_fisicos_array[i]);
        }
        free(bloques_fisicos_array);
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    int bloque_fisico_actual = atoi(bloques_fisicos_array[bloque_logico]);

    config_destroy(metadata);
    if (bloques_fisicos_array) {
        for (int i = 0; bloques_fisicos_array[i] != NULL; i++) free(bloques_fisicos_array[i]);
        free(bloques_fisicos_array);
    }
    free(metadata_path);

    // 3. Verificar hard links (si es unico, escribir directamente. Si no, obtener uno nuevo y copiar).
    char* nombre_bloque_logico = string_from_format("%06d.dat", bloque_logico);
    char* ruta_bloque_logico = string_from_format("%s/files/%s/%s/logical_blocks/%s",
                                                 storage->punto_montaje, nombre_file, tag, nombre_bloque_logico);
    free(nombre_bloque_logico);

    struct stat st;
    if (stat(ruta_bloque_logico, &st) != 0) {
        log_error(storage->logger, "No se pudo obtener info del bloque lógico %s", ruta_bloque_logico);
        free(ruta_bloque_logico);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    bool es_unico_hardlink = (st.st_nlink == 1);
    free(ruta_bloque_logico);

    int bloque_fisico_final = bloque_fisico_actual;

    if (!es_unico_hardlink) {
        // a. Buscar un nuevo bloque fisico libre
        int nuevo_bloque_fisico = -1;
        int cantidad_bloques = storage->tamanio_filesystem / storage->tamanio_bloque;

        pthread_mutex_lock(&storage->mutex_bitmap);
        
        for (int i = 1; i < cantidad_bloques; i++) { 
            //empieza en 1, el 0 es initial_file
            if (!bitarray_test_bit(storage->bitmap, i)) {
                nuevo_bloque_fisico = i;
                break;
            }
        }
        if (nuevo_bloque_fisico == -1) {
            log_error(storage->logger, "Espacio Insuficiente - No hay bloques físicos libres para escritura diferenciada");
            pthread_mutex_unlock(&storage->mutex_bitmap);
            pthread_mutex_unlock(file_mutex);
            return false;
        }

        // b. Marcar nuevo bloque como ocupado temporalmente (en caso de error, revertir)
        bitarray_set_bit(storage->bitmap, nuevo_bloque_fisico);

        pthread_mutex_unlock(&storage->mutex_bitmap);

        log_info(storage->logger, "Bloque físico %d reservado para escritura diferenciada", nuevo_bloque_fisico);

        // c. Obtener rutas de los bloques fisico origen y destino
        char* ruta_fisico_actual = string_from_format("%s/physical_blocks/block%04d.dat", storage->punto_montaje, bloque_fisico_actual);
        char* ruta_fisico_nuevo = string_from_format("%s/physical_blocks/block%04d.dat", storage->punto_montaje, nuevo_bloque_fisico);

        // d. Copiar contenido del bloque fisico actual al nuevo
        FILE* f_origen = fopen(ruta_fisico_actual, "rb");
        FILE* f_destino = fopen(ruta_fisico_nuevo, "wb");
        if (!f_origen || !f_destino) {
            log_error(storage->logger, "Error al abrir bloques físicos para copia en escritura diferenciada");
            if (f_origen) fclose(f_origen);
            if (f_destino) fclose(f_destino);
            // Revertir: marcar nuevo bloque como libre
            bitarray_clean_bit(storage->bitmap, nuevo_bloque_fisico);
            free(ruta_fisico_actual);
            free(ruta_fisico_nuevo);
            pthread_mutex_unlock(file_mutex);
            return false;
        }
        //suponiendo tamanio fijo de bloque, pq no cambia en tiempo de ejecucion
        void* buffer_copia = malloc(storage->tamanio_bloque);
        if (fread(buffer_copia, 1, storage->tamanio_bloque, f_origen) > 0) {
            fwrite(buffer_copia, 1, storage->tamanio_bloque, f_destino);
        }
        free(buffer_copia);
        fclose(f_origen);
        fclose(f_destino);

        // e. Eliminar el hard link viejo del bloque logico
        char* nombre_bloque_logico_upd = string_from_format("%06d.dat", bloque_logico);
        char* ruta_bloque_logico_upd = string_from_format("%s/files/%s/%s/logical_blocks/%s",
                                                         storage->punto_montaje, nombre_file, tag, nombre_bloque_logico_upd);
        free(nombre_bloque_logico_upd);

        if (unlink(ruta_bloque_logico_upd) != 0) {
            log_error(storage->logger, "Error al eliminar hard link viejo en escritura diferenciada");
            //revertir cambios
            bitarray_clean_bit(storage->bitmap, nuevo_bloque_fisico);
            free(ruta_fisico_actual);
            free(ruta_fisico_nuevo);
            free(ruta_bloque_logico_upd);
            pthread_mutex_unlock(file_mutex);
            return false;
        }
        log_info(storage->logger, "##%d-%s:%s Se eliminó el hard link del bloque lógico %d al bloque físico %d",
                 query_id, nombre_file, tag, bloque_logico, bloque_fisico_actual);

        // f. Crear nuevo hard link del bloque logico al nuevo bloque físico
        if (link(ruta_fisico_nuevo, ruta_bloque_logico_upd) != 0) {
             log_error(storage->logger, "Error al crear nuevo hard link en escritura diferenciada");
             // Revertir cambios
             bitarray_clean_bit(storage->bitmap, nuevo_bloque_fisico);
             // Intentar recrear el link viejo (no es perfecto, pero se intenta)
             link(ruta_fisico_actual, ruta_bloque_logico_upd);
             free(ruta_fisico_actual);
             free(ruta_fisico_nuevo);
             free(ruta_bloque_logico_upd);
             pthread_mutex_unlock(file_mutex);
             return false;
        }
        log_info(storage->logger, "##%d-%s:%s Se agregó el hard link del bloque lógico %d al bloque físico %d",
                 query_id, nombre_file, tag, bloque_logico, nuevo_bloque_fisico);

        free(ruta_bloque_logico_upd);
        free(ruta_fisico_actual);
        free(ruta_fisico_nuevo);

        bloque_fisico_final = nuevo_bloque_fisico;

        // g. Actualizar metadata.config con el nuevo bloque fisico
        metadata_path = string_from_format("%s/files/%s/%s/metadata.config",
                                          storage->punto_montaje, nombre_file, tag);
        metadata = config_create(metadata_path);
        if (!metadata) {
            log_error(storage->logger, "No se pudo recargar metadata para actualizar bloque en escritura diferenciada");
            free(metadata);
            free(metadata_path);
            pthread_mutex_unlock(file_mutex);
            return false; 
        }

        char** bloques_actuales = config_get_array_value(metadata, "BLOCKS");
        int len = get_array_length(bloques_actuales);
        if (bloques_actuales && bloque_logico < len) {
            // Actualizar el indice correspondiente
            free(bloques_actuales[bloque_logico]); //liberar string anterior
            bloques_actuales[bloque_logico] = string_itoa(bloque_fisico_final); // Asignar nuevo valor
            // Serializar de nuevo la lista completa
            char* bloques_serializados = string_new();
            string_append(&bloques_serializados, "[");
            for (int i = 0; i < len; i++) {
                string_append(&bloques_serializados, bloques_actuales[i]);
                if (i < len - 1) string_append(&bloques_serializados, ",");
            }
            string_append(&bloques_serializados, "]");
            config_set_value(metadata, "BLOCKS", bloques_serializados);
            config_save(metadata);
            free(bloques_serializados);
        }
        config_destroy(metadata);
        if (bloques_actuales) {
            for (int i = 0; bloques_actuales[i] != NULL; i++) free(bloques_actuales[i]);
            free(bloques_actuales);
        }
        free(metadata_path);

    } // Fin del bloque para escritura diferenciada


    // 4. Escribir el contenido en el bloque fisico final (ya sea el original o el nuevo)
    char* ruta_fisico_final = string_from_format("%s/physical_blocks/block%04d.dat", storage->punto_montaje, bloque_fisico_final);
    FILE* f_bloque_final = fopen(ruta_fisico_final, "r+b"); // r+ para leer/escribir, b para binario
    if (!f_bloque_final) {
        log_error(storage->logger, "No se pudo abrir el bloque físico %d para escritura", bloque_fisico_final);
        free(ruta_fisico_final);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // Aplicar retardos
    usleep(storage->retardo_operacion * 10);
    usleep(storage->retardo_acceso_bloque * 10);

    // Escribir el nuevo contenido
    fseek(f_bloque_final, 0, SEEK_SET); // Ir al inicio del bloque
    fwrite(contenido, 1, tamanio_contenido, f_bloque_final);
    // Rellenar el resto con ceros si es necesario
    if (tamanio_contenido < storage->tamanio_bloque) {
        char cero = 0;
        for (int i = tamanio_contenido; i < storage->tamanio_bloque; i++) {
            fwrite(&cero, 1, 1, f_bloque_final);
        }
    }
    fclose(f_bloque_final);
    free(ruta_fisico_final);

    // 5. Log obligatorio
    log_info(storage->logger, "##%d- Bloque Lógico Escrito %s:%s - Número de Bloque: %d",
             query_id, nombre_file, tag, bloque_logico);

    pthread_mutex_unlock(file_mutex);
    return true;
}


//*****************************************************************************
// Calcula el MD5 de un bloque físico individual (.bin)
char* calcular_md5_por_bloque(const char* path_bloque, int tamanio_bloque)
{
    FILE* file = fopen(path_bloque, "rb");
    if (!file) {
        printf("No se pudo abrir el bloque para calcular MD5: %s\n", path_bloque);
        return NULL;
    }

    // Leer el contenido completo del bloque
    unsigned char* buffer = malloc(tamanio_bloque);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t bytes_leidos = fread(buffer, 1, tamanio_bloque, file);
    fclose(file);

    // Calcular hash con la función provista por utils
    char* md5_bloque = crypto_md5((char*)buffer, bytes_leidos);

    free(buffer);
    return md5_bloque; // el caller libera
}

//*****************************************************************************
// Evita duplicidad de bloques al commitear un file:tag.
// Compara hashes de cada bloque contra blocks_hash_index.config
void evitar_duplicidad(t_storage* storage, char* file, char* tag) {
    log_info(storage->logger, "Iniciando proceso de deduplicación para %s:%s", file, tag);

    // 1. Cargar metadata.config del file:tag
    char* metadata_path = string_from_format("%s/files/%s/%s/metadata.config",
                                           storage->punto_montaje, file, tag);
    if (access(metadata_path, F_OK) != 0) {
        log_error(storage->logger, "No se puede deduplicar, metadata.config no existe para %s:%s", file, tag);
        free(metadata_path);
        return;
    }

    t_config* metadata = config_create(metadata_path);
    if (!metadata) {
        log_error(storage->logger, "No se pudo cargar metadata.config para %s:%s", file, tag);
        free(metadata_path);
        return;
    }

    //lock
    pthread_mutex_lock(&storage->mutex_hash_index);

    // 2. Obtener bloques actuales del metadata
    char** bloques_str = config_get_array_value(metadata, "BLOCKS");
    int cantidad_bloques = get_array_length(bloques_str);
    if (cantidad_bloques == 0 || !bloques_str) {
        log_info(storage->logger, "No hay bloques para deduplicar en %s:%s", file, tag);
        config_destroy(metadata);
        if (bloques_str) string_array_destroy(bloques_str);
        free(metadata_path);
        return;
    }

    // 3. Cargar o crear blocks_hash_index.config
    char* hash_index_path = string_from_format("%s/blocks_hash_index.config", storage->punto_montaje);
    //bool hash_index_creado = false;
    
    // Verificar si el archivo existe, si no, crearlo vacío 
    if (access(hash_index_path, F_OK) != 0) {
        FILE* f = fopen(hash_index_path, "w");
        if (!f) {
            log_error(storage->logger, "No se pudo crear el archivo blocks_hash_index.config en %s", hash_index_path);
            config_destroy(metadata);
            string_array_destroy(bloques_str);
            free(metadata_path);
            free(hash_index_path);
            pthread_mutex_unlock(&storage->mutex_hash_index);
            return;
        }
        fclose(f);
        //hash_index_creado = true;
    }

    t_config* hash_index = config_create(hash_index_path);
    if (!hash_index) {
        log_error(storage->logger, "No se pudo cargar blocks_hash_index.config");
        config_destroy(metadata);
        string_array_destroy(bloques_str);
        free(metadata_path);
        free(hash_index_path);
        pthread_mutex_unlock(&storage->mutex_hash_index);
        return;
    }

    // 4. Iterar sobre cada bloque lógico (índice) y su bloque físico actual
    bool se_modifico_metadata = false;
    char** nuevos_bloques_str = malloc((cantidad_bloques + 1) * sizeof(char*)); // Nuevo array para bloques actualizados
    if (!nuevos_bloques_str) {
        log_error(storage->logger, "Error al malloc de nuevo array de bloques en evitar_duplicidad");
        config_destroy(metadata);
        config_destroy(hash_index);
        string_array_destroy(bloques_str);
        free(metadata_path);
        free(hash_index_path);
        pthread_mutex_unlock(&storage->mutex_hash_index);
        return;
    }
    
    for (int i = 0; i < cantidad_bloques; i++) {
        if (!bloques_str[i] || strlen(bloques_str[i]) == 0) {
            log_warning(storage->logger, "Bloque lógico %d tiene valor vacío en %s:%s", i, file, tag);
            nuevos_bloques_str[i] = string_duplicate(bloques_str[i]); // Copiar el vacío o NULL
            continue;
        }
        
        int bloque_fisico_actual = atoi(bloques_str[i]);
        if (bloque_fisico_actual < 0) {
            log_warning(storage->logger, "Bloque lógico %d tiene valor inválido (%d) en %s:%s", 
                        i, bloque_fisico_actual, file, tag);
            nuevos_bloques_str[i] = string_duplicate(bloques_str[i]); // Copiar el valor inválido
            continue;
        }

        // Calcular el hash del bloque físico actual
        char* path_bloque_fisico = string_from_format("%s/physical_blocks/block%04d.dat",
                                                    storage->punto_montaje, bloque_fisico_actual);
        
        if (access(path_bloque_fisico, F_OK) != 0) {
            log_warning(storage->logger, "Bloque físico %d no existe en el filesystem para %s:%s", 
                        bloque_fisico_actual, file, tag);
            free(path_bloque_fisico);
            nuevos_bloques_str[i] = string_duplicate(bloques_str[i]); // Copiar el valor original
            continue;
        }
        
        char* hash = calcular_md5_por_bloque(path_bloque_fisico, storage->tamanio_bloque);
        if (!hash) {
            log_error(storage->logger, "No se pudo calcular hash para bloque físico %d en %s:%s", 
                     bloque_fisico_actual, file, tag);
            free(path_bloque_fisico);
            nuevos_bloques_str[i] = string_duplicate(bloques_str[i]); // Copiar el valor original
            continue;
        }

        // 5. Verificar si el hash ya existe en el índice
        if (config_has_property(hash_index, hash)) {
            // Ya existe un bloque con este contenido
            char* bloque_existente_str = config_get_string_value(hash_index, hash); // no entendi como busca la palabra clave "hash", si dentro del blocks_hash_index.config estan los hashes como claves
            int bloque_existente = -1;
            
            // Extraer el número del bloque existente desde "blockXXXX"
            if (bloque_existente_str && strlen(bloque_existente_str) > 5) {
                bloque_existente = atoi(bloque_existente_str + 5); // Extraer número de "block0000"
            }

            if (bloque_existente >= 0 && bloque_existente != bloque_fisico_actual) {
                log_info(storage->logger, "Deduplicación: Bloque lógico %d de %s:%s reasignado de %d a %d (hash: %s)",
                        i, file, tag, bloque_fisico_actual, bloque_existente, hash);

                // a. Actualizar el bloque en la metadata (nuevo array)
                nuevos_bloques_str[i] = string_itoa(bloque_existente);
                se_modifico_metadata = true;

                // b. Actualizar el enlace lógico para que apunte al bloque físico existente
                char* ruta_logico = string_from_format("%s/files/%s/%s/logical_blocks/%06d.dat",
                                                     storage->punto_montaje, file, tag, i);
                
                // Eliminar el enlace actual
                if (unlink(ruta_logico) == 0) {
                    // Crear nuevo enlace al bloque físico existente
                    char* path_bloque_existente = string_from_format("%s/physical_blocks/block%04d.dat",
                                                                   storage->punto_montaje, bloque_existente);
                    
                    if (link(path_bloque_existente, ruta_logico) != 0) {
                        log_error(storage->logger, "Error al crear nuevo enlace para el bloque lógico %d en %s:%s", 
                                 i, file, tag);
                        // Intentar restaurar el enlace original
                        char* path_bloque_original = string_from_format("%s/physical_blocks/block%04d.dat",
                                                                      storage->punto_montaje, bloque_fisico_actual);
                        link(path_bloque_original, ruta_logico);
                        free(path_bloque_original);
                        // Si falla restaurar, copiamos el contenido del existente al original
                        // Esto es más robusto que dejar un enlace roto
                        FILE* src = fopen(path_bloque_existente, "rb");
                        FILE* dst = fopen(path_bloque_fisico, "wb"); // Sobrescribe el original con el existente
                        if (src && dst) {
                            void* buffer = malloc(storage->tamanio_bloque);
                            if (buffer) {
                                size_t leidos;
                                while ((leidos = fread(buffer, 1, storage->tamanio_bloque, src)) > 0) {
                                    fwrite(buffer, 1, leidos, dst);
                                }
                                free(buffer);
                            }
                            fclose(src);
                            fclose(dst);
                            nuevos_bloques_str[i] = string_itoa(bloque_fisico_actual); // Mantener el bloque original
                            se_modifico_metadata = false; // Indicar que no se modificó realmente
                        } else {
                            if (src) fclose(src);
                            if (dst) fclose(dst);
                        }
                    } else {
                        log_info(storage->logger, "Enlace lógico %d actualizado correctamente en %s:%s", 
                                i, file, tag);
                    }
                    free(path_bloque_existente);
                } else {
                    log_error(storage->logger, "Error al eliminar enlace original para el bloque lógico %d en %s:%s", 
                             i, file, tag);
                    nuevos_bloques_str[i] = string_duplicate(bloques_str[i]); // Copiar el valor original
                }
                free(ruta_logico);

                // c. Liberar el bloque físico original en el bitmap
                pthread_mutex_lock(&storage->mutex_bitmap);
                if (bitarray_test_bit(storage->bitmap, bloque_fisico_actual)) {
                    bitarray_clean_bit(storage->bitmap, bloque_fisico_actual);
                    log_info(storage->logger, "##0-%s:%s Bloque Físico Liberado- Número de Bloque: %d",
                            file, tag, bloque_fisico_actual);
                    
                    // Verificar si el bloque físico no tiene más enlaces y eliminarlo
                    struct stat st;
                    if (stat(path_bloque_fisico, &st) == 0 && st.st_nlink == 1) { // Solo si es el único enlace
                        if (remove(path_bloque_fisico) == 0) {
                            log_info(storage->logger, "Bloque físico %d eliminado físicamente (sin enlaces)", 
                                    bloque_fisico_actual);
                        }
                    }
                }
                pthread_mutex_unlock(&storage->mutex_bitmap);
            } else if (bloque_existente < 0) {
                log_warning(storage->logger, "Valor inválido para bloque existente en hash %s: %s", 
                           hash, bloque_existente_str);
                nuevos_bloques_str[i] = string_duplicate(bloques_str[i]); // Copiar el valor original
            } else {
                 // El bloque ya era el correcto, solo loggear y copiar
                log_info(storage->logger, "Bloque lógico %d de %s:%s ya estaba correctamente apuntando al bloque físico %d (hash: %s)",
                        i, file, tag, bloque_existente, hash);
                nuevos_bloques_str[i] = string_duplicate(bloque_existente_str); // Copiar el valor correcto
            }
        } else {
            // No existe, agregarlo al índice
            char* valor_bloque = string_from_format("block%04d", bloque_fisico_actual);
            config_set_value(hash_index, hash, valor_bloque);
            log_info(storage->logger, "##0-%s:%s Nuevo hash agregado al índice: %s -> %s",
                    file, tag, hash, valor_bloque);
            free(valor_bloque);
            nuevos_bloques_str[i] = string_duplicate(bloques_str[i]); // Copiar el valor original
        }

        free(hash);
        free(path_bloque_fisico);
    }

    // 6. Serializar y guardar la nueva lista de bloques en metadata (solo si hubo cambios)
    if (se_modifico_metadata) {
        nuevos_bloques_str[cantidad_bloques] = NULL; // Asegurar terminación del array
        char* bloques_serializados = serializar_bloques_array(nuevos_bloques_str);
        config_set_value(metadata, "BLOCKS", bloques_serializados);
        config_save(metadata);
        free(bloques_serializados);
    }

    // 7. Guardar el índice de hashes actualizado
    config_save(hash_index);

    log_info(storage->logger, "Proceso de deduplicación completado para %s:%s", file, tag);

    // 8. Liberar recursos
    config_destroy(metadata);
    config_destroy(hash_index);
    string_array_destroy(bloques_str);
    if (nuevos_bloques_str) {
        for (int i = 0; i < cantidad_bloques; i++) {
            if (nuevos_bloques_str[i]) free(nuevos_bloques_str[i]);
        }
        free(nuevos_bloques_str);
    }
    free(metadata_path);
    free(hash_index_path);

    //unlock
    pthread_mutex_unlock(&storage->mutex_hash_index);
}


//*****************************************************************************
// Guarda el estado actual del bitmap en disco
void persistir_bitmap(t_storage* storage) {
    
    pthread_mutex_lock(&storage->mutex_bitmap);
    
    FILE* f = fopen(storage->path_bitmap, "wb");
    if (!f) {
        log_error(storage->logger, "No se pudo abrir el bitmap para persistir");
        return;
    }
    fwrite(storage->bitmap->bitarray, 1, storage->bitmap->size, f);
    fclose(f);
    log_info(storage->logger, "Bitmap persistido correctamente");

    pthread_mutex_unlock(&storage->mutex_bitmap);
}

//*****************************************************************************
//Verifica si un archivo/tag tiene estado COMMITED en su archivo .cfg
bool verificar_si_commited(t_storage* storage, const char* file, const char* tag) {
    // 1️⃣ Armar la ruta del archivo .cfg del file:tag
    char* path_cfg = string_from_format("%s/files/%s/%s.config",
                                        storage->punto_montaje, file, tag);

    // 2️⃣ Intentar abrir el archivo de configuración
    t_config* cfg = config_create(path_cfg);
    if (!cfg) {
        log_warning(storage->logger,
                    "No se pudo abrir el archivo de configuración para verificar commit: %s",
                    path_cfg);
        free(cfg);
        free(path_cfg);
        return false; // No está committeado si no existe
    }

    // 3️⃣ Leer el campo STATUS del .cfg
    const char* status = config_get_string_value(cfg, "STATUS");
    bool esta_commited = false;

    if (status != NULL && string_equals_ignore_case((char*)status, "COMMITED")) {
        esta_commited = true;
        log_warning(storage->logger,
                    "El archivo %s:%s está COMMITED. Operación no permitida.",
                    file, tag);
    }

    // 4️⃣ Liberar recursos
    config_destroy(cfg);
    free(path_cfg);

    return esta_commited;
}


//*****************************************************************************
bool realizar_commit(t_storage* storage, t_list* parametros) {
    if (!parametros || list_size(parametros) < 4) { // [op, query_id, file, tag]
        log_error(storage->logger, "Parámetros insuficientes para STORAGE_COMMIT");
        return false;
    }

    int query_id = *(int*)list_get(parametros, 1);
    char* file = (char*)list_get(parametros, 2);
    char* tag  = (char*)list_get(parametros, 3);

    // Obtener lock en el diccionario
    pthread_mutex_t* file_mutex = get_or_create_file_mutex(storage, file, tag);
    pthread_mutex_lock(file_mutex);

    log_info(storage->logger, "Iniciando commit para %s:%s", file, tag);

    // 1. Obtener ruta del metadata.config
    char* metadata_path = string_from_format("%s/files/%s/%s/metadata.config",
                                           storage->punto_montaje, file, tag);

    if (access(metadata_path, F_OK) != 0) {
        log_error(storage->logger, "File:Tag %s:%s no existe para commit", file, tag);
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // 2. Cargar metadata
    t_config* metadata = config_create(metadata_path);
    if (!metadata) {
        log_error(storage->logger, "No se pudo abrir metadata.config para commit de %s:%s", file, tag);
        free(metadata);
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // 3. Verificar estado actual
    char* estado_actual = config_get_string_value(metadata, "ESTADO");
    if (estado_actual && string_equals_ignore_case(estado_actual, "COMMITED")) {
        log_warning(storage->logger, "El File:Tag %s:%s ya está COMMITED", file, tag);
        config_destroy(metadata);
        free(metadata_path);
        pthread_mutex_unlock(file_mutex);
        return true; // Considerar como éxito si ya está commited
    }

    // 4. Realizar deduplicación
    log_info(storage->logger, "Realizando deduplicación antes del commit para %s:%s", file, tag);
    evitar_duplicidad(storage, file, tag); // Llama a la versión corregida

    // 5. Actualizar estado a COMMITED
    config_set_value(metadata, "ESTADO", "COMMITED");
    config_save(metadata);
    config_destroy(metadata);

    log_info(storage->logger, "##%d- Commit de File:Tag %s:%s", query_id, file, tag);

    // 6. Persistir bitmap (esto puede hacerse aquí o en otro momento, dependiendo del diseño)
    persistir_bitmap(storage);

    free(metadata_path);
    pthread_mutex_unlock(file_mutex);
    return true;
}


// ****************************************************************************
bool eliminar_file_tag(t_storage* storage, int query_id, const char* file, const char* tag) {
    // 1️⃣ Armar rutas
    char* path_tag = string_from_format("%s/files/%s/%s", storage->punto_montaje, file, tag);
    char* path_metadata = string_from_format("%s/metadata.config", path_tag);
    char* path_bitmap = string_from_format("%s/bitmap.bin", storage->punto_montaje);

    // Obtener lock en el diccionario
    pthread_mutex_t* file_mutex = get_or_create_file_mutex(storage, file, tag);
    pthread_mutex_lock(file_mutex);

    // 2️⃣ Verificar existencia
    if (access(path_tag, F_OK) != 0) {
        log_warning(storage->logger, "Intento de eliminar File:Tag inexistente %s:%s", file, tag);
        free(path_tag);
        free(path_metadata);
        free(path_bitmap);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // 3️⃣ Abrir metadata
    t_config* metadata = config_create(path_metadata);
    if (metadata == NULL) {
        log_error(storage->logger, "No se pudo abrir metadata de %s:%s", file, tag);
        free(metadata);
        free(path_tag);
        free(path_metadata);
        free(path_bitmap);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // 4️⃣ Obtener bloques
    char** bloques = config_get_array_value(metadata, "BLOCKS"); // <-- Esto devuelve un array de strings dinámicos

    // 5️⃣ Liberar bloques físicos
    for (int i = 0; bloques != NULL && bloques[i] != NULL; i++) {
        int num_bloque = atoi(bloques[i]);
        marcar_bloque_libre(storage, query_id, num_bloque);
    }

    // 6️⃣ Guardar bitmap actualizado en disco
    FILE* f = fopen(path_bitmap, "wb");
    if (f) {
        fwrite(storage->bitmap->bitarray, storage->bitmap->size, 1, f);
        fclose(f);
    } else {
        log_error(storage->logger, "No se pudo abrir %s para persistir bitmap", path_bitmap);
    }

    // 7️⃣ Destruir metadata
    config_destroy(metadata); // <-- Esto libera la estructura t_config, pero no el array bloques

    // 8️⃣ Liberar el array de bloques obtenido de config_get_array_value
    if (bloques != NULL) {
        for (int i = 0; bloques[i] != NULL; i++) {
            free(bloques[i]); // <-- Liberar cada string del array
        }
        free(bloques); // <-- Liberar el array de punteros
    }

    // 9️⃣ Borrar carpeta física del tag
    int rm_ok = rm_rf(path_tag);
    if (rm_ok != 0) {
        log_error(storage->logger, "No se pudo borrar %s (codigo %d)", path_tag, rm_ok);
        free(path_tag);
        free(path_metadata);
        free(path_bitmap);
        pthread_mutex_unlock(file_mutex);
        return false;
    }

    // 🔟 Log final de éxito
    log_info(storage->logger, "##%d- File Eliminado %s:%s", query_id, file, tag);

    // 1️⃣1️⃣ Liberar memoria temporal
    free(path_tag);
    free(path_metadata);
    free(path_bitmap);
    pthread_mutex_unlock(file_mutex);
    return true;
}

//*****************************************************************************
char* serializar_bloques_array(char** bloques) {
    if (!bloques || !bloques[0]) {
        return string_duplicate("[]");
    }

    // Contar elementos
    int count = 0;
    while(bloques[count] != NULL) count++;

    if (count == 0) {
        return string_duplicate("[]");
    }

    char* resultado = string_new();
    string_append(&resultado, "[");

    for(int i = 0; i < count; i++) {
        string_append(&resultado, bloques[i]);
        if(i < count - 1) {
            string_append(&resultado, ",");
        }
    }

    string_append(&resultado, "]");
    return resultado;
}


//*****************************************************************************
pthread_mutex_t* get_or_create_file_mutex(t_storage* storage, const char* file, const char* tag) {
    char* file_tag_id = string_from_format("%s:%s", file, tag);
    
    pthread_mutex_lock(&storage->mutex_dict_locks);
    
    // 1. Buscar el lock
    pthread_mutex_t* file_mutex = dictionary_get(storage->dict_locks_files, file_tag_id);
    
    // 2. Si no existe, crearlo y agregarlo
    if (file_mutex == NULL) {
        file_mutex = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(file_mutex, NULL);
        dictionary_put(storage->dict_locks_files, file_tag_id, file_mutex);
        log_debug(storage->logger, "Mutex creado para %s", file_tag_id);
    }
    
    pthread_mutex_unlock(&storage->mutex_dict_locks);
    
    free(file_tag_id);
    return file_mutex;
}


//*****************************************************************************
void remove_file_mutex(t_storage* storage, const char* file, const char* tag) {
    char* file_tag_id = string_from_format("%s:%s", file, tag);
    
    pthread_mutex_lock(&storage->mutex_dict_locks);
    
    // 1. Remover de la lista y obtener el dato (el mutex)
    void* data = dictionary_remove(storage->dict_locks_files, file_tag_id);
    
    // 2. Si se encontró, destruir sus recursos
    if (data != NULL) {
        pthread_mutex_t* file_mutex = (pthread_mutex_t*) data;
        pthread_mutex_destroy(file_mutex);
        free(file_mutex);
        log_debug(storage->logger, "Mutex destruido para %s", file_tag_id);
    }
    
    pthread_mutex_unlock(&storage->mutex_dict_locks);
    free(file_tag_id);
}