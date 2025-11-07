#include "storage.h"
#include <sys/stat.h>       // Para stat()
#include <commons/string.h> // Para string_*, get_array_length


// ****************************************************************************
// Obtener la longitud de un array de strings
static int get_array_length(char** array) {
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

// MarcarBloqueLibre Papu
/* 
    char* nombre_archivo = strrchr(path_fisico, '/');
    if (!nombre_archivo) return;

    int nro_bloque = atoi(nombre_archivo + 6); // saltea los primeros 6 caracteres "/block" dejando 0005.dat por ejemplo
    bitarray_clean_bit(storage->bitmap, nro_bloque);
    log_info(storage->logger, "Bloque físico %d marcado como libre en el bitmap", nro_bloque);

    msync(storage->bitmap->bitarray, storage->bitmap->size, MS_SYNC); // sincronizo el bitmap con los cambios

*/

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
    fprintf(metadata_file, "BLOCKS=[]\n");  // <-- CORRECTO: "BLOCKS", NO "BLOQUES"
    fclose(metadata_file);
    free(metadata_path);

    usleep(storage->retardo_operacion * 10);  //retardo olbigatorio

    // Log obligatorio con QUERY_ID
    log_info(storage->logger, "##%d- File Creado %s:%s", query_id, nombre_file, tag_inicial);

    free(ruta_abs_tag);
    return true;
}
static char* path_logico_para_truncate(const char* punto_montaje, const char* nombre_file, const char* tag, int i) {
    return string_from_format("%s/files/%s/%s/logical_blocks/block%06d.dat",
                              punto_montaje, nombre_file, tag, i);
}

static char* path_fisico_para_truncate(const char* punto_montaje, int bloque_fisico_id) {
    return string_from_format("%s/physical_blocks/block%04d.dat", punto_montaje, bloque_fisico_id);
}

static int* leer_bloques_actuales(t_config* metadata_config, int* cantidad_bloques_fisico) {
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
         free(array);
         return NULL; // Debe devolver NULL para indicar 0 bloques
    }

    //caso 2: La lista tiene elementos validos
    int n = 0;
    while(array[n] != NULL && strlen(array[n]) > 0) { // Asegura que no sea string vacío
        n++;
    }
    if(n == 0) { //si el primer elemento es "" o hay solo strings vacios
        printf("Leer bloques actuales: Conteo de bloques es 0 (solo strings vacíos)");
        free(array);
        return NULL; //devolver NULL si no hay bloques válidos
    }
    int* array_bloques = malloc(sizeof(int) * n);
    if (!array_bloques) {
        printf("No se pudo allocar memoria para array_bloques en leer_bloques_actuales");
        free(array); //liberar el array original
        return NULL;
    }
    for(int i = 0; i < n; i++) {
        array_bloques[i] = atoi(array[i]);
    }
    *cantidad_bloques_fisico = n;
    free(array); //liberar el array de strings devuelto por config_get_array_value
    return array_bloques;
}

static char* serializar_bloques(const int* bloques, int cantidad_bloques) {
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

    //la ruta correcta es .../files/nombre_file/tag/metadata.config
    char* ruta_metadata = string_from_format("%s/files/%s/%s/metadata.config", storage->punto_montaje, nombre_file, tag);

    if(access(ruta_metadata, F_OK) != 0) {
        log_error(storage->logger, "El tag %s del file %s no existe", tag, nombre_file);
        free(ruta_metadata); //se libera la ruta antes de retornar
        return false;
    }

    t_config* metadata_config = config_create(ruta_metadata);
    if(!metadata_config){
        log_error(storage->logger, "No se pudo abrir el metadata.config para truncar el file %s tag %s", nombre_file, tag);
        free(ruta_metadata); //liberar la ruta antes de retornar
        return false;
    }

    int tamanio_actual = config_get_int_value(metadata_config, "TAMANIO");
    int tamanio_bloque = storage->tamanio_bloque;
    
    //división entera hacia arriba para obtener bloques actuales/nuevos
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
            
            //crear directorio lógico si no existe
            char* dir_logico = string_substring_until(path_logico, string_length(path_logico) - 10); // Quitar "blockXXXXXX.dat"
            mkdir_recursivo(dir_logico);
            free(dir_logico);

            if(link(path_fisico, path_logico) != 0) {
                log_error(storage->logger, "Error al crear link para el %s -> %s", path_logico, path_fisico);
                free(path_logico);
                free(path_fisico);
                config_destroy(metadata_config);
                free(ruta_metadata); //liberar la ruta antes de retornar
                return false;
            }
            log_info(storage->logger, "Bloque logico %d creado y linkeado con el bloque fisico 0", i);
            free(path_logico);
            free(path_fisico);
        }
    }

    //disminuir tamaño
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
                    free(ruta_metadata); //liberar la ruta antes de retornar
                    return false;
                }
            }
            free(path_logico);
            char* path_fisico = path_fisico_para_truncate(storage->punto_montaje, array_fisico_id);
            struct stat st;
            if (stat(path_fisico, &st) == 0) {
                if(st.st_nlink == 1) { //si es el ultimo link
                    marcar_bloque_libre(storage, query_id, array_fisico_id);
                    log_info(storage->logger, "Bloque fisico %d liberado", array_fisico_id);
                }
            }
            free(path_fisico);
        }
    }

    //reconstruir la lista de bloques físicos para la nueva cantidad
    int cantidad_bloques_fisico_nueva = bloques_nuevos;
    int* bloque_fisico_final = NULL;
    if(cantidad_bloques_fisico_nueva > 0) {
        bloque_fisico_final = malloc(sizeof(int) * cantidad_bloques_fisico_nueva);
        if (!bloque_fisico_final) {
            log_error(storage->logger, "Error al allocar memoria para bloque_fisico_final en truncar_file");
            config_destroy(metadata_config);
            free(ruta_metadata);
            if(array_bloques_fisico) free(array_bloques_fisico); // <-- Liberar array_bloques_fisico si no es NULL
            return false;
        }
        int copiar = (cantidad_bloques_fisico < cantidad_bloques_fisico_nueva ? cantidad_bloques_fisico : cantidad_bloques_fisico_nueva);
        
        //verificar si array_bloques_fisico no es NULL antes de copiar
        if (array_bloques_fisico != NULL) {
            for(int i = 0; i < copiar; i++) {
                bloque_fisico_final[i] = array_bloques_fisico[i];
            }
        } else {
            //OJO
            //Si array_bloques_fisico es NULL, pero copiar > 0, es un error de logica.
            //Esto implicaria que cantidad_bloques_fisico > 0 pero array_bloques_fisico es NULL.
            //En este caso, no hay bloques antiguos que copiar, así que los nuevos bloques
            //se inicializan a 0 en el siguiente bucle.
            //No se hace nada aca, el bucle de inicializacion abajo se encarga
            //de los nuevos bloques.
        }
        for(int i = copiar; i < cantidad_bloques_fisico_nueva; i++) {
            bloque_fisico_final[i] = 0; //nuevo bloque, apunta a 0 inicialmente
        }
    }

    //actualizar metadata
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

    if (!nombre_file || !tag || strlen(nombre_file) == 0 || strlen(tag) == 0) {
        log_error(storage->logger, "Nombre de File o Tag inválido");
        return false;
    }

    char* ruta_tag = string_from_format("%s/files/%s/%s", storage->punto_montaje, nombre_file, tag);
    if (access(ruta_tag, F_OK) != 0) {
        log_error(storage->logger, "File:Tag inexistente: %s:%s", nombre_file, tag);
        free(ruta_tag);
        return false;
    }
    free(ruta_tag);

    char* metadata_path = string_from_format("%s/files/%s/%s/metadata.config",
                                            storage->punto_montaje, nombre_file, tag);
    t_config* metadata = config_create(metadata_path);
    if (!metadata) {
        log_error(storage->logger, "No se pudo cargar metadata de %s:%s", nombre_file, tag);
        free(metadata_path);
        return false;
    }

    int tam_archivo = config_get_int_value(metadata, "TAMANIO");
    int bloques_logicos_totales = (tam_archivo + storage->tamanio_bloque - 1) / storage->tamanio_bloque;

    if (bloque_logico < 0 || bloque_logico >= bloques_logicos_totales) {
        log_error(storage->logger, "Lectura fuera de límite: bloque lógico %d en archivo de %d bytes",
                  bloque_logico, tam_archivo);
        config_destroy(metadata);
        free(metadata_path);
        return false;
    }

    config_destroy(metadata);
    free(metadata_path);

    char* nombre_bloque = string_from_format("block%06d.dat", bloque_logico);
    char* ruta_bloque_logico = string_from_format("%s/files/%s/%s/logical_blocks/%s",
                                                 storage->punto_montaje, nombre_file, tag, nombre_bloque);
    free(nombre_bloque);

    FILE* f_bloque = fopen(ruta_bloque_logico, "r"); //deberia ser "rb" si son binarios
    if (!f_bloque) {
        log_error(storage->logger, "No se encontró el bloque lógico: %s", ruta_bloque_logico);
        free(ruta_bloque_logico);
        return false;
    }

    *contenido = malloc(storage->tamanio_bloque);
    if (!*contenido) {
        log_error(storage->logger, "Error al allocar memoria para contenido de bloque en leer_bloque");
        fclose(f_bloque);
        free(ruta_bloque_logico);
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

    if (!nombre_file || !tag || strlen(nombre_file) == 0 || strlen(tag) == 0) {
        log_error(storage->logger, "Nombre de File o Tag inválido");
        return false;
    }

    // 1. Verificar existencia del File:Tag
    char* ruta_tag = string_from_format("%s/files/%s/%s", storage->punto_montaje, nombre_file, tag);
    if (access(ruta_tag, F_OK) != 0) {
        log_error(storage->logger, "Intento de escritura en File:Tag inexistente: %s:%s", nombre_file, tag);
        free(ruta_tag);
        return false;
    }
    free(ruta_tag);

    // 2. Cargar metadata
    char* metadata_path = string_from_format("%s/files/%s/%s/metadata.config",
                                            storage->punto_montaje, nombre_file, tag);
    t_config* metadata = config_create(metadata_path);
    if (!metadata) {
        log_error(storage->logger, "No se pudo cargar metadata de %s:%s", nombre_file, tag);
        free(metadata_path);
        return false;
    }

    char* estado_str = config_get_string_value(metadata, "ESTADO");
    if (strcmp(estado_str, "COMMITED") == 0) {
        log_error(storage->logger, "Intento de escritura en File:Tag COMMITED: %s:%s", nombre_file, tag);
        config_destroy(metadata);
        free(metadata_path);
        return false;
    }

    int tam_archivo = config_get_int_value(metadata, "TAMANIO");
    int bloques_logicos_totales = (tam_archivo + storage->tamanio_bloque - 1) / storage->tamanio_bloque;

    if (bloque_logico < 0 || bloque_logico >= bloques_logicos_totales) {
        log_error(storage->logger, "Escritura fuera de límite: bloque lógico %d en archivo de %d bytes",
                  bloque_logico, tam_archivo);
        config_destroy(metadata);
        free(metadata_path);
        return false;
    }

    // Obtener lista de bloques fisicos actuales
    char** bloques_fisicos_array = config_get_array_value(metadata, "BLOCKS");
    if (!bloques_fisicos_array || !bloques_fisicos_array[0]) { 
        // Verificar si la lista está vacia o es NULL
        log_error(storage->logger, "Metadata de %s:%s no tiene bloques físicos asignados", nombre_file, tag);
        config_destroy(metadata);
        if (bloques_fisicos_array) free(bloques_fisicos_array); //liberar el array de strings
        free(metadata_path);
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
    char* nombre_bloque_logico = string_from_format("block%06d.dat", bloque_logico);
    char* ruta_bloque_logico = string_from_format("%s/files/%s/%s/logical_blocks/%s",
                                                 storage->punto_montaje, nombre_file, tag, nombre_bloque_logico);
    free(nombre_bloque_logico);

    struct stat st;
    if (stat(ruta_bloque_logico, &st) != 0) {
        log_error(storage->logger, "No se pudo obtener info del bloque lógico %s", ruta_bloque_logico);
        free(ruta_bloque_logico);
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
        char* nombre_bloque_logico_upd = string_from_format("block%06d.dat", bloque_logico);
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
            free(metadata_path);
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
    
    return true;
}

//*****************************************************************************
// Calcula el hash MD5 de un archivo y lo devuelve como string hexadecimal
char* calcular_md5_de_archivo(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("No se pudo abrir el archivo para calcular MD5: %s\n", path);
        return NULL;
    }

    //Leer todo el contenido del archivo
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    if (size <= 0) {
        fclose(file);
        return strdup("d41d8cd98f00b204e9800998ecf8427e");  // MD5 de archivo vacío
    }

    char* buffer = malloc(size);
    fread(buffer, 1, size, file);
    fclose(file);

    // Calcular el hash MD5
    char* md5_string = crypto_md5(buffer, size);

    free(buffer);
    return md5_string;
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
bool realizar_commit(t_storage* storage, t_list* parametros) {

    // Obtener ruta del archivo desde los parámetros recibidos
    char* path_rel = (char*)list_get(parametros, 1);
    char* path_abs = obtener_ruta_absoluta(path_rel);

    // Calcular el MD5 del archivo real
    char* md5 = calcular_md5_de_archivo(path_abs);
    if (!md5) {
        log_error(storage->logger, "Error al calcular MD5 de %s", path_rel);
        free(path_abs);
        return false;
    }

    // Crear o abrir el archivo .cfg correspondiente
    char* path_cfg = string_from_format("%s.cfg", path_abs);
    t_config* cfg = config_create(path_cfg);
    if (!cfg) {
        log_error(storage->logger, "No se pudo abrir archivo de configuración: %s", path_cfg);
        free(md5);
        free(path_cfg);
        free(path_abs);
        return false;
    }

    // Actualizar valores del config
    config_set_value(cfg, "MD5", md5);
    config_set_value(cfg, "STATUS", "COMMITED");
    config_save(cfg);

    // Persistir bitmap y cerrar config
    persistir_bitmap(storage);
    config_destroy(cfg);

    log_info(storage->logger, "Commit completado para %s (MD5: %s)", path_rel, md5);
    free(md5);
    free(path_cfg);
    free(path_abs);
    return true;
}

// ****************************************************************************
bool eliminar_file_tag(t_storage* storage, int query_id, const char* file, const char* tag) {
    // 1️⃣ Armar rutas
    char* path_tag = string_from_format("%s/files/%s/%s", storage->punto_montaje, file, tag);
    char* path_metadata = string_from_format("%s/metadata.config", path_tag);
    char* path_bitmap = string_from_format("%s/bitmap.bin", storage->punto_montaje);

    // 2️⃣ Verificar existencia
    if (access(path_tag, F_OK) != 0) {
        log_warning(storage->logger, "Intento de eliminar File:Tag inexistente %s:%s", file, tag);
        free(path_tag);
        free(path_metadata);
        free(path_bitmap);
        return false;
    }

    // 3️⃣ Abrir metadata
    t_config* metadata = config_create(path_metadata);
    if (metadata == NULL) {
        log_error(storage->logger, "No se pudo abrir metadata de %s:%s", file, tag);
        free(path_tag);
        free(path_metadata);
        free(path_bitmap);
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
        return false;
    }

    // 🔟 Log final de éxito
    log_info(storage->logger, "##%d- File Eliminado %s:%s", query_id, file, tag);

    // 1️⃣1️⃣ Liberar memoria temporal
    free(path_tag);
    free(path_metadata);
    free(path_bitmap);
    return true;
}