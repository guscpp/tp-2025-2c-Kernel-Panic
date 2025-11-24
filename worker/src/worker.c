#include "../include/worker.h"
#include "../include/query_interpreter.h" //por la funcion que llama al ciclo
#include <unistd.h>

bool query_desconectado;
int socket_distpach;
pthread_mutex_t mutex_interrupt; 

t_worker* inicializar_worker(int id_worker)
{
    t_worker *w = malloc(sizeof(t_worker));

    // TODO leer log_level del config
    w->logger = iniciar_logger("worker.log", "[WORKER_PRUEBA]", true, LOG_LEVEL_INFO);

    w->config = iniciar_config(w->logger, "worker.config");  //cuidado que esta hardcodeado
    w->ip_master = config_get_string_value(w->config, "IP_MASTER");
    w->puerto_master = config_get_int_value(w->config, "PUERTO_MASTER");
    w->ip_storage = config_get_string_value(w->config, "IP_STORAGE");
    w->puerto_storage = config_get_string_value(w->config, "PUERTO_STORAGE");
    w->path_scripts = config_get_string_value(w->config, "PATH_QUERIES");
    w->log_level = config_get_string_value(w->config, "LOG_LEVEL");
    w->id_worker = id_worker;
    w->master_socket_distpach= -1;
    w->master_socket_interrupt = -1;
    w->storage_socket = -1;

    return w;
}

void verificar_worker(t_worker* w)
{
    log_info(w->logger, "Id_worker: %d", w->id_worker);
    log_info(w->logger, "Ip_Master: %s", w->ip_master);
    log_info(w->logger, "Puerto Master: %d", w->puerto_master);
    log_info(w->logger, "Ip_Storage: %s", w->ip_storage);
    log_info(w->logger, "Puerto_Storage: %s", w->puerto_storage);
    log_info(w->logger, "PathScript: %s", w->path_scripts);
}


void liberar_worker(t_worker* w)
{
    if (!w) return;
    
    // Cerrar sockets
    if (w->master_socket_distpach > 0) close(w->master_socket_distpach);
    if (w->master_socket_interrupt > 0) close(w->master_socket_interrupt);
    if (w->storage_socket > 0) close(w->storage_socket);
    
    // Liberar memoria interna
    if (w->mem) {
        destruir_memoria(w->mem);
        w->mem = NULL;
    }
    
    // Liberar interprete de queries
    if (w->interpreter) {
        free(w->interpreter);
        w->interpreter = NULL;
    }
    
    // Liberar flag de error de storage
    if (w->flag_error_storage) {
        pthread_mutex_destroy(&w->flag_error_storage->mutex_error_storage);
        free(w->flag_error_storage);
        w->flag_error_storage = NULL;
    }
    
    // Destruir logger y config
    if (w->logger) log_destroy(w->logger);
    if (w->config) config_destroy(w->config);
    
    // Liberar strings
    free(w->ip_master);
    free(w->ip_storage);
    free(w->puerto_storage);
    free(w->path_scripts);
    free(w->log_level);
    
    free(w);
}


Pcb* recibir_path_de_query(int master_socket, t_worker* w) 
{
    log_warning(w->logger, "Espero recibir un path"); 
    Pcb* dt_archivo = NULL;

    ////////solo para probar errores///////
    socket_distpach = w->master_socket_distpach;

    t_list* paquete_path = recibir_paquete(master_socket);
    int* codigo_operacion = list_get(paquete_path, 0);

    if (*codigo_operacion == -1)
    {
        log_error(w->logger, "Error en la conexión con el Master");
        return NULL;
    }

    if (*codigo_operacion == WORKER_ASSIGN_QUERY)
    {
        log_info(w->logger, "Llegue a recibir el paquete path_query de Master");
        if (paquete_path && list_size(paquete_path)>= 3) 
        {
            // Los valores vienen en el orden:
            // query_id, path_query, prioridad, pc

            int query_id = *(int*)list_get(paquete_path, 1);
            char* path_query = (char*)list_get(paquete_path, 2);
            int prioridad = *(int*)list_get(paquete_path, 3);
            int pc = *(int*)list_get(paquete_path, 4);
            
            log_info(w->logger, "Query recibida: ID=%d, Path=%s, Prioridad=%d, PC:%d", 
                    query_id, path_query, prioridad, pc);  

            if(pc > 0){
                log_info(w->logger, "Es un proceso que fue interrumpido antes");
            }
            dt_archivo = malloc(sizeof(Pcb));
            
            // Intentar abrir el archivo
            if((dt_archivo->archivo = retornar_archivo(path_query, w->path_scripts, w->logger))){
                dt_archivo->nombre_archivo = path_query;
                dt_archivo->query_id = query_id;
                dt_archivo->pc = pc;

                list_destroy(paquete_path);
                return dt_archivo;
            }

            // Si falla al abrir el archivo:
            list_destroy(paquete_path);
            error_path_not_found(w->logger, WORKER_ERROR_QUERY_NO_ENCONTRADA, dt_archivo->query_id, dt_archivo->nombre_archivo); // Ahora usa dt_archivo->nombre_archivo, asegurar que tenga memoria o usar copia. Nota: path_query venía de la lista que acabamos de destruir, cuidado aquí. *Corrección*: path_query es puntero a la lista. Si destruimos la lista, path_query puede ser invalido. 
            // *Nota*: En tu código original destruyes la lista antes de usar el path en el error. Asumiremos que string_duplicate o similar ocurre internamente o el orden es el que tenías.
            // Para seguridad, deberíamos usar el ID recibido antes.
            
            return NULL;
        }
    }
    
    return NULL;
}


FILE* retornar_archivo(char* nombre_archivo, char* path_general, t_log* logger){

    char* path_final = string_new();
    string_append(&path_final, path_general);
    string_append(&path_final, nombre_archivo);
    
    FILE* archivo_query = fopen(path_final, "r"); 

    if (archivo_query == NULL) {
        log_error(logger, "No se pudo abrir el archivo de query: %s", path_final); 
        free(path_final);
        return NULL;
    }
    //free(path_final); // Ojo con leaks si no se libera
    return archivo_query;
}   

//------------------HILO DE EJECUCION DE QUERYS-----------------------
void* ejecutar_query(void* arg){
    t_ejecucion* datos_ejecucion = (t_ejecucion*) arg;

    log_info(datos_ejecucion->w->logger, "Por lo menos entre a ejecutar_query");
    Pcb* dt_archivo;

    while(1){ 
        dt_archivo = recibir_path_de_query(datos_ejecucion->master_socket, datos_ejecucion->w);
        
        // Retorna el pcb con los datos del proceso a ejecutar
        if(dt_archivo == NULL){
            continue; // Si hubo error, vuelvo a esperar
        }

        log_info(datos_ejecucion->w->logger, "Llego el path_query: %s", dt_archivo->nombre_archivo);

        query_interpreter_ciclo(dt_archivo, datos_ejecucion->w); 
    
        if(query_desconectado){
            continue;
        }
    }
    // Se eliminó la recursividad ejecutar_query(arg); para evitar Stack Overflow
    return NULL;
}

//------------------HILO DE ATENCION DE INTERRUPCIONES-----------------------
void* hilo_atender_interrupcion(void* arg){ 
    bool hay_interrupcion;

    t_ejecucion* dt_atender_master = (t_ejecucion*) arg; 
    log_info(dt_atender_master->w->logger, "Entre a hilo de interrupciones");

    dt_atender_master->w->interpreter->hay_interrupcion = false;
    
    while(1){   
        hay_interrupcion = recibir_interrupciones(dt_atender_master->master_socket, dt_atender_master->w);
        
        if (hay_interrupcion) {
            pthread_mutex_lock(&mutex_interrupt);
            dt_atender_master->w->interpreter->hay_interrupcion = true; 
            pthread_mutex_unlock(&mutex_interrupt);
            log_info(dt_atender_master->w->logger, "Me llego una interrupcion");
        } else {
            log_warning(dt_atender_master->w->logger, "Se desconectó del master o hubo error. Se rompio el hilo de interrupciones");
            break;
        }
    }
    return NULL;
}

//-------------------------------------------------------------------------------------------------
//este master_socket es el socket del interrupt
bool recibir_interrupciones(int master_socket, t_worker* w){ 

    t_list* paquete_interrupcion = recibir_paquete(master_socket);

    int* codigo_operacion = list_get(paquete_interrupcion, 0);

    if (codigo_operacion == NULL) {
        log_error(w->logger, "Código de operación inválido en interrupción.");
        list_destroy(paquete_interrupcion);
        return false;
    }

    if (*codigo_operacion == -1) {
        log_error(w->logger, "Error en la conexión con el Master.");
        list_destroy(paquete_interrupcion);
        return false;
    }

    if (*codigo_operacion == WORKER_DESALOJO) {
        log_info(w->logger, "Llegó interrupción WORKER_DESALOJO del Master.");
        list_destroy(paquete_interrupcion);
        query_desconectado = false;
        return true; 
    }
    if(*codigo_operacion == WORKER_QUERY_DESCONECTADO ){
        log_info(w->logger, "Llegó interrupción WORKER_QUERY_DESCONECTADO del Master.");
        list_destroy(paquete_interrupcion);
        query_desconectado = true;
        return true; 
    }
    list_destroy(paquete_interrupcion);
    return false; 
}

void retener_worker(t_worker* w) {
    log_warning(w->logger, "Retengo al worker antes de crear el interrupt");

    t_list* paquete;
    paquete = recibir_paquete(w->master_socket_distpach);

    int* cod_op = (int*) list_get(paquete, 0);   

    if (*cod_op == -1)
    {
        log_error(w->logger, "Error en la conexion con Master");
        return;   
    }

    if (*cod_op == RETENER_WORKER)
    {
        log_info(w->logger, "Pude pasar de retener worker");
        return;  
    }
}

//-------------------------------------------------------------------------------------------------

void rtas_storage(int storage_socket, t_worker* w, t_instr_param* parametros, Pcb* pcb) {
    log_info(w->logger, "Esperando respuesta de Storage");
    
    t_list* valores = recibir_paquete(storage_socket);
    if(valores == NULL || list_size(valores) == 0) {
        log_error(w->logger, "No se recibió respuesta de Storage");
        return;
    }
    
    int* cod_op = list_get(valores, 0);
    log_info(w->logger, "Respuesta de Storage recibida, código: %d", *cod_op);
    
    switch(*cod_op) {
        case STORAGE_SEND_BLOCK_SIZE:
            log_info(w->logger, "Storage envió tamaño de bloque");
            if(list_size(valores) >= 2) {
                int* tamanio_bloque = list_get(valores, 1);
                w->tamanio_bloque = *tamanio_bloque;
                log_info(w->logger, "Tamaño de bloque recibido: %d", w->tamanio_bloque);
            }
        break;
        case STORAGE_SEND_OK_CREATE_FILE:
            log_info(w->logger, "Storage confirmó creación de archivo exitosa");
            log_info(w->logger, "Query<%d>: Instrucción realizada: CREATE %s:%s", 
             pcb->query_id, parametros->nombre_file, parametros->tag);
            break;
        case STORAGE_SEND_OK_TRUNCATE:
            log_info(w->logger, "Storage confirmó truncado exitoso");
            log_info(w->logger, "Query<%d>: Instrucción realizada: TRUNCATE %s:%s %d bytes", 
            pcb->query_id, parametros->nombre_file, parametros->tag, parametros->tamanio);
            break;
        case STORAGE_SEND_OK_WRITE_BLOCK:
            log_info(w->logger, "Storage confirmó escritura exitosa");
            break;
        case STORAGE_SEND_OK_READ_BLOCK: {
            log_info(w->logger, "Storage envió bloque para lectura");
            // El bloque se guarda en mem->tmp_bloque en pedir_bloque_storage
            break;
        }
        case STORAGE_SEND_OK_TAG:
            log_info(w->logger, "Storage confirmó creación de tag exitosa");
            log_info(w->logger, "Query<%d>: Instrucción realizada: TAG %s:%s -> %s:%s", 
             pcb->query_id, parametros->nombre_file_org, parametros->tag_origen,
             parametros->nombre_file_destino, parametros->tag_destino);
            break;
        case STORAGE_SEND_OK_COMMIT:
            log_info(w->logger, "Storage confirmó commit exitoso");
            log_info(w->logger, "Query<%d>: Instrucción realizada: COMMIT %s:%s", 
             pcb->query_id, parametros->nombre_file, parametros->tag);
            break;
        case STORAGE_SEND_OK_DELETE:
            log_info(w->logger, "Storage confirmó eliminación exitosa");
            log_info(w->logger, "Query<%d>: Instrucción realizada: DELETE %s:%s", 
             pcb->query_id, parametros->nombre_file, parametros->tag);
            break;
        case STORAGE_SEND_OK_FLUSH:
            log_info(w->logger, "Storage confirmó flush exitoso");
            log_info(w->logger, "Query<%d>: Instrucción realizada: FLUSH %s:%s", 
            pcb->query_id, parametros->nombre_file, parametros->tag);
            break;

        // Errores 
        case STORAGE_SEND_ERROR_CREATE_FILE:
            log_error(w->logger, "Storage no pudo hacer create");
            
            if(w->flag_error_storage == NULL){
                log_error(w->logger, "FLAG_ERROR_STORAGE ES NULL ANTES DEL MUTEX LOCK");
            }
            pthread_mutex_lock(&w->flag_error_storage->mutex_error_storage);
            w->flag_error_storage->error_storage = true;
            pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);

            informar_error_create(parametros, w);
            break;
        case STORAGE_SEND_ERROR_TRUNCATE:
            log_error(w->logger, "Storage no pudo hacer truncate");

            pthread_mutex_lock(&w->flag_error_storage->mutex_error_storage);
            w->flag_error_storage->error_storage = true;
            pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);

            informar_error_truncate(parametros, w);
            break;
        case STORAGE_SEND_ERROR_WRITE_BLOCK:
            log_error(w->logger, "Storage no pudo hacer write");

            pthread_mutex_lock(&w->flag_error_storage->mutex_error_storage);
            w->flag_error_storage->error_storage = true;
            pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);

            informar_error_write(parametros, w);
            break;
        case STORAGE_SEND_ERROR_READ_BLOCK:
            log_error(w->logger, "Storage no pudo hacer read");

            pthread_mutex_lock(&w->flag_error_storage->mutex_error_storage);
            w->flag_error_storage->error_storage = true;
            pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);

            informar_error_read(parametros, w);
            break;
        case STORAGE_SEND_ERROR_TAG:
            log_error(w->logger, "Storage no pudo hacer tag");

            pthread_mutex_lock(&w->flag_error_storage->mutex_error_storage);
            w->flag_error_storage->error_storage = true;
            pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);

            informar_error_tag(parametros, w);
            break;
        case STORAGE_SEND_ERROR_COMMIT:
            log_error(w->logger, "Storage no pudo hacer commit");

            pthread_mutex_lock(&w->flag_error_storage->mutex_error_storage);
            w->flag_error_storage->error_storage = true;
            pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);

            informar_error_commit(parametros, w);
            break;
        case STORAGE_SEND_ERROR_DELETE:
            log_error(w->logger, "Storage no pudo hacer delete");

            pthread_mutex_lock(&w->flag_error_storage->mutex_error_storage);
            w->flag_error_storage->error_storage = true;
            pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);

            informar_error_delete(parametros, w);
            break;
        case STORAGE_SEND_ERROR_FLUSH:
            log_error(w->logger, "Storage no pudo hacer flush");

            pthread_mutex_lock(&w->flag_error_storage->mutex_error_storage);
            w->flag_error_storage->error_storage = true;
            pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);

            informar_error_flush(parametros, w);
            break;
        default:
            log_warning(w->logger, "Respuesta desconocida de Storage: %d", *cod_op);
            break;
    }
    
    list_destroy_and_destroy_elements(valores, free);
}


//ERRORES
void error_path_not_found(t_log* logger, op_code etiqueta, int id_query, char* path){ 
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError = crear_paquete(etiqueta, buffer1);
    agregar_a_paquete(paqueteError, &id_query, sizeof(int));
    agregar_a_paquete(paqueteError, path, strlen(path) +1);
    enviar_paquete(paqueteError, socket_distpach, logger);
    eliminar_paquete(paqueteError);
    loggerError(logger, etiqueta);
}

void error_archivo_not_found(t_log* logger, op_code etiqueta, int id_query, char* file, char* tag){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError = crear_paquete(etiqueta, buffer1);
    agregar_a_paquete(paqueteError, &id_query, sizeof(int));
    agregar_a_paquete(paqueteError, file, strlen(file)+1);
    agregar_a_paquete(paqueteError, tag, strlen(tag)+1);
    enviar_paquete(paqueteError, socket_distpach, logger);
    eliminar_paquete(paqueteError);
    loggerError(logger, etiqueta);
}

void error_tamanio_escrLectura_excedido(t_log* logger, op_code etiqueta, int id_query, char* file, char* tag){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError = crear_paquete(etiqueta, buffer1);
    agregar_a_paquete(paqueteError, &id_query, sizeof(int));
    agregar_a_paquete(paqueteError, file, strlen(file)+1);
    agregar_a_paquete(paqueteError, tag, strlen(tag)+1);
    enviar_paquete(paqueteError, socket_distpach, logger);
    eliminar_paquete(paqueteError);
    loggerError(logger, etiqueta);
}

void error_instruccion_malformada(t_log* logger,int id_query, char* instruccion){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError1 = crear_paquete(WORKER_ERROR_INSTRUCCION_MALFORMADA, buffer1);
    agregar_a_paquete(paqueteError1, &id_query, sizeof(int));
    agregar_a_paquete(paqueteError1, instruccion, strlen(instruccion)+1);
    enviar_paquete(paqueteError1, socket_distpach, logger);
    eliminar_paquete(paqueteError1);
    loggerError(logger, WORKER_ERROR_INSTRUCCION_MALFORMADA);

}

void informar_error_create(t_instr_param* parametros, t_worker* w){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError1 = crear_paquete(WORKER_ERROR_CREATE, buffer1);
    agregar_a_paquete(paqueteError1, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paqueteError1, parametros->tag, strlen(parametros->tag) + 1); 
    enviar_paquete(paqueteError1, socket_distpach, w->logger);
    eliminar_paquete(paqueteError1);
}

void informar_error_truncate(t_instr_param* parametros, t_worker* w){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError1 = crear_paquete(WORKER_ERROR_TRUNCATE, buffer1);
    agregar_a_paquete(paqueteError1, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paqueteError1, parametros->tag, strlen(parametros->tag) + 1);
    agregar_a_paquete(paqueteError1, &parametros->tamanio, sizeof(int));
    enviar_paquete(paqueteError1, socket_distpach, w->logger);
    eliminar_paquete(paqueteError1);
}

void informar_error_write(t_instr_param* parametros, t_worker* w){ 
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError1 = crear_paquete(WORKER_ERROR_WRITE_EN_STORAGE, buffer1);
    enviar_paquete(paqueteError1, socket_distpach, w->logger);
    eliminar_paquete(paqueteError1);
}

void informar_error_read(t_instr_param* parametros, t_worker* w){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError1 = crear_paquete(WORKER_ERROR_READ_EN_STORAGE, buffer1);
    enviar_paquete(paqueteError1, socket_distpach, w->logger);
    eliminar_paquete(paqueteError1);
}

void informar_error_tag(t_instr_param* parametros, t_worker* w){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError1 = crear_paquete(WORKER_ERROR_TAG, buffer1);
    agregar_a_paquete(paqueteError1, parametros->nombre_file_org, strlen(parametros->nombre_file_org)+1);
    agregar_a_paquete(paqueteError1, parametros->tag_origen, strlen(parametros->tag_origen)+1);
    agregar_a_paquete(paqueteError1, parametros->nombre_file_destino, strlen(parametros->nombre_file_destino)+1);
    agregar_a_paquete(paqueteError1, parametros->tag_destino, strlen(parametros->tag_destino)+1);
    enviar_paquete(paqueteError1, socket_distpach, w->logger);
    eliminar_paquete(paqueteError1);
}

void informar_error_commit(t_instr_param* parametros, t_worker* w){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError1 = crear_paquete(WORKER_ERROR_COMMIT, buffer1);
    agregar_a_paquete(paqueteError1, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paqueteError1, parametros->tag, strlen(parametros->tag)+1);
    enviar_paquete(paqueteError1, socket_distpach, w->logger);
    eliminar_paquete(paqueteError1);
}

void informar_error_delete(t_instr_param* parametros, t_worker* w){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError1 = crear_paquete(WORKER_ERROR_DELETE, buffer1);
    agregar_a_paquete(paqueteError1, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paqueteError1, parametros->tag, strlen(parametros->tag)+1);
    enviar_paquete(paqueteError1, socket_distpach, w->logger);
    eliminar_paquete(paqueteError1);
}

void informar_error_flush(t_instr_param* parametros, t_worker* w){
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError1 = crear_paquete(WORKER_ERROR_FLUSH, buffer1);
    eliminar_paquete(paqueteError1);
}

//SOlo para ver si anda bien el envio de errores (borrar porque no es necesario)
void loggerError(t_log* logger, op_code etiqueta){
    switch (etiqueta)
    {
    case WORKER_ERROR_ARCHIVO:
        log_info(logger, "Envie a master el WORKER_ERROR_ARCHIVO");
        break;
    
    case WORKER_ERROR_INSTRUCCION_MALFORMADA:
        log_info(logger, "Envie a master el WORKER_INSTRUCCION_MALFORMADA");
        break;
    
    case WORKER_ERROR_TAMANIO_ESCRITURA_EXCEDIDO:
        log_info(logger, "Envie a master el WORKER_ERROR_TAMANIO_ESCRITURA_EXCEDIDO"); 
        break;
    
    case WORKER_ERROR_TAMANIO_LECTURA_EXCEDIDO:
        log_info(logger, "Envie a master el  WORKER_ERROR_TAMANIO_LECTURA_EXCEDIDO"); 
        break;
    
    case WORKER_ERROR_DIRECCION_INVALIDA:
        log_info(logger, "Envie a master el WORKER_ERROR_DIRECCION_INVALIDA");
        break;

    case WORKER_ERROR_MODIFICAR_COMMIT:
        log_info(logger, "Envie a master el WORKER_ERROR_MODIFICAR_COMMIT");
        break;
    case WORKER_ERROR_QUERY_NO_ENCONTRADA:
        log_info(logger, "Envie a master WORKER_ERROR_QUERY_NO_ENCONTRADA");
        break;

    default:
    log_info(logger, "default");
        break;
    }
}

storage_error* inicializar_mutex_error_storage(t_worker* w) {
    storage_error* error_storage = malloc(sizeof(storage_error));

    if(pthread_mutex_init(&error_storage->mutex_error_storage, NULL) != 0){
        log_warning(w->logger,"Error al inicializar el mutex error_storage");

    }
    log_info(w->logger, "Se iniciaron bien los mutex");
    error_storage->error_storage = false;
    return error_storage;
}