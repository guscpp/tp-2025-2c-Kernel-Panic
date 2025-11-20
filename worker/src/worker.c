#include "../include/worker.h"
#include "../include/query_interpreter.h"
#include "../include/tipos.h"
#include <unistd.h>


int socket_distpach;
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
    // log_info(w->logger, "Tam_memoria: %d", w->tamanio_memoria);
    // log_info(w->logger, "Retardo_Memoria: %d", w->retardo_memoria);
    // log_info(w->logger, "Algoritmos_reemplazo: %s", w->algoritmo_reemplazo);
    log_info(w->logger, "PathScript: %s", w->path_scripts);
}


void liberar_worker(t_worker* w)
{
    if (w) {
        // if (w->master_socket != NULL) close(w->master_socket);
        // if (w->storage_socket != NULL) close(w->storage_socket);
        if (w->logger) log_destroy(w->logger);
        if (w->config) config_destroy(w->config);
        free(w);
    }
}


Pcb* recibir_path_de_query(int master_socket, t_worker* w) 
{
    log_warning(w->logger, "ESpero recibir un path"); 
    Pcb* dt_archivo = NULL;

    ////////solo para probar errores///////
    socket_distpach = w->master_socket_distpach;

    t_list* paquete_path = recibir_paquete(master_socket);
    int* codigo_operacion =  list_get(paquete_path, 0);

    if (*codigo_operacion == -1)
    {
        log_error(w->logger, "Error en la conexión con el Master");
        return NULL;
    }

    if (*codigo_operacion == WORKER_ASSIGN_QUERY)
    {

        log_info(w->logger, "Llegue a recibir el paquete path_query de MAster");
        if (paquete_path && list_size(paquete_path)>= 3) 
        {
            // Los valores vienen en el orden:
            // query_id, path_query, prioridad, pc

            //Por el error de utils lo comento y hardcodeo valores
            int query_id = *(int*)list_get(paquete_path, 1);
            char* path_query = (char*)list_get(paquete_path, 2);
            int prioridad = *(int*)list_get(paquete_path, 3); //(*)
            int pc = *(int*)list_get(paquete_path, 4);

            
            log_info(w->logger, "Query recibida: ID=%d, Path=%s, Prioridad=%d, PC:%d", 
                    query_id, path_query, prioridad, pc);  

            if(pc > 0){
                log_info(w->logger, "Es un proceso que fue interrumpido antes");
            }
            dt_archivo = malloc(sizeof(Pcb));
            if((dt_archivo->archivo = retornar_archivo(path_query, w->path_scripts, w->logger))){

                dt_archivo->nombre_archivo = path_query;
                dt_archivo->query_id = query_id;
                dt_archivo->pc = pc;

                list_destroy(paquete_path);
                return dt_archivo;
            }

            list_destroy(paquete_path);
            error_path_not_found(w->logger, WORKER_ERROR_QUERY_NO_ENCONTRADA, dt_archivo->query_id);

            return NULL;
            //free(path_query); //?
        }
    
    }
    
    return NULL;
}


FILE* retornar_archivo(char* nombre_archivo, char* path_general, t_log* logger){

    char* path_final = string_new();
    string_append(&path_final, path_general);
    string_append(&path_final, nombre_archivo);
    
    FILE* archivo_query = fopen(path_final, "r"); //lo pruebo desde worker/

    if (archivo_query == NULL) {
        log_error(logger, "No se pudo abrir el archivo de query: %s", path_final); 
        free(path_final);
        return NULL;
    }
    //free(path_final);
    return archivo_query;
}   

//------------------HILO DE EJECUCION DE QUERYS-----------------------
void* ejecutar_query(void* arg){
    // te agrego NULL a los returns porque la funcion espera un void* (linea 210)
    // se podria modificar a void sin el asterisco?

    t_ejecucion* datos_ejecucion = (t_ejecucion*) arg;

    log_info(datos_ejecucion->w->logger, "Por lo menos entre a ejecutar_query");
    Pcb* dt_archivo;

    while((dt_archivo = recibir_path_de_query(datos_ejecucion->master_socket, datos_ejecucion->w))){ 
        //retorna el pcb con los datos del proceso a ejecutar

        log_info(datos_ejecucion->w->logger, "Llego el path_query: %s", dt_archivo->nombre_archivo);
        /* creo que ya no va, pero quiero probar mas cosas dsp
        if(dt_archivo == NULL){
            log_info(datos_ejecucion->w->logger, "Error al abrir query, estoy en worker.c");
            query_interpreter_ciclo(dt_archivo, datos_ejecucion->w); 
            return NULL;
        }
        */
        query_interpreter_ciclo(dt_archivo, datos_ejecucion->w); 
    
    }
    ejecutar_query(arg);
    return NULL;
}

//------------------HILO DE ATENCION DE INTERRUPCIONES-----------------------
void* hilo_atender_interrupcion(void* arg){ //Cuando me lleguen interrupciones, las guardo en el interpreter que tiene un booleano encargado de chquear eso. Y en el ciclo siempre revisamos ese bool
    
    bool hay_interrupcion;

    t_ejecucion* dt_atender_master = (t_ejecucion*) arg; 
    log_info(dt_atender_master->w->logger, "ENtre a hilo de interrupciones");

    dt_atender_master->w->interpreter->hay_interrupcion = false;
    
    while(1){   //RECORDAR que recibir_interrupciones tiene master_socket pero se refiere al socket del interrupt (no al socket generico de antes)
    hay_interrupcion = recibir_interrupciones(dt_atender_master->master_socket, dt_atender_master->w);//solo devuelve true si es cierto
        
        if (hay_interrupcion) {
            //comento-mutex pthread_mutex_lock(&mutex_interrupt);
            dt_atender_master->w->interpreter->hay_interrupcion = true; //aca marcamos en true la interrupcion para verificarlo despues en el ciclo de instrucciones  
            //comento-mutex pthread_mutex_unlock(&mutex_interrupt);
            log_info(dt_atender_master->w->logger, "Me llego una interrupcion");
        } else {
            log_warning(dt_atender_master->w->logger, "Se desconectó del master o hubo error. Se rOmpio el hilo de interrupciones");
            break;
        }
            
    }
    return NULL;
}
//-------------------------------------------------------------------------------------------------
//este master_socket es el socket del interrupt
bool recibir_interrupciones(int master_socket, t_worker* w){ //SOlo se encarga de devolver true en el caso de que llegue una interrupcion

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


void rtas_storage(int storage_socket, t_worker* w){    
    log_info(w->logger, "Lllegue a rts_storage");
        t_list* valores = recibir_paquete(storage_socket);
        int* cod_op = list_get(valores, 0);
        log_info(w->logger, "llegue a recibir %d", *cod_op);
/*
        if(*cod_op == STORAGE_SEND_OK){
            log_info(w->logger, "LA instruccion se hizo bien");
            return;
        }

        if(*cod_op == STORAGE_SEND_ERROR){
            *cod_op = de_intruccion;
        }
*/
        switch (*cod_op)
        {
        case STORAGE_SEND_BLOCK_SIZE:
            int* size = list_get(valores, 1);
            w->tamanio_bloque = *size;
            log_info(w->logger, "Tamaño de bloque recibido: %d", w->tamanio_bloque);
            list_destroy_and_destroy_elements(valores, free);
            break;
/*
        case CREATE:
            log_info(w->logger, "Storage devolvio error al hacer create");
            break;
            
        case TRUNCATE:
            log_info(w->logger, "Storage devolvio error al hacer truncate");
            break;

        case WRITE:
            log_info(w->logger, "Storage devolvio error al hacer write");
            break;

        case READ:
            log_info(w->logger, "Storage devolvio error al hacer read");
            break;

        case TAG:
            log_info(w->logger, "Storage devolvio error al hacer tag");
            break;
        case COMMIT:
            log_info(w->logger, "Storage devolvio error al hacer commit");
            avisar_error_generico(w->logger, WORKER_ERROR_MODIFICAR_COMMIT);
        break;
        case FLUSH:
            log_info(w->logger, "Storage devolvio error al hacer flush");
            break;
        case DELETE:
            log_info(w->logger, "Storage devolvio error al hacer delete");
            break;
*/
        default:
        log_info(w->logger, "Error en el cod_op %d", *cod_op);
            break;
        }
}


//ERRORES
void error_path_not_found(t_log* logger, op_code etiqueta, int id_query){ //descomentar para el envio
    
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError = crear_paquete(etiqueta, buffer1);
    agregar_a_paquete(paqueteError, &id_query, sizeof(int));
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
//SOlo para ver si anda bien el envio de errores
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
        log_info(logger, "Envie a master el WORKER_ERROR_TAMANIO_ESCRITURA_EXCEDIDO");    //este error es porque si yo quiero leer o escribir en memoria, tengo que asegurar de que el offset + el tamanio de lo que escribo o leo este dentro del tamanio de pagina
        break;
    
    case WORKER_ERROR_TAMANIO_LECTURA_EXCEDIDO:
        log_info(logger, "Envie a master el  WORKER_ERROR_TAMANIO_LECTURA_EXCEDIDO");    //este error es porque si yo quiero leer o escribir en memoria, tengo que asegurar de que el offset + el tamanio de lo que escribo o leo este dentro del tamanio de pagina
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
