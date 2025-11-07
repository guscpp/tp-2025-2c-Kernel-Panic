#include "../include/worker.h"
#include "../include/query_interpreter.h"
#include "../include/tipos.h"
#include <unistd.h>


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
            //int pc = *(int*)list_get(paquete_path, 4);  el PC va venir cuarto

            int pc = 0; //pongo este porque en el codigo master todavia no me manda el pc
            
            log_info(w->logger, "Query recibida: ID=%d, Path=%s, Prioridad=%d, PC:%d", 
                    query_id, path_query, prioridad, pc);  

            if(pc > 0){
                log_info(w->logger, "Es un proceso que fue interrumpido antes");
            }
            dt_archivo = malloc(sizeof(Pcb));
            if(dt_archivo->archivo = retornar_archivo(path_query, w->path_scripts, w->logger)){

                dt_archivo->nombre_archivo = path_query;
                dt_archivo->query_id = query_id;
                dt_archivo->pc = pc;

                list_destroy(paquete_path);
                return dt_archivo;
            }

            list_destroy(paquete_path);
            avisar_error_generico(w, WORKER_ERROR_ARCHIVO);

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
//-------------------------------------------------------------------------------------------------


void rtas_storage(int storage_socket, t_worker* w){     //cambiar
        t_list* valores = recibir_paquete(storage_socket);
        int* cod_op = list_get(valores, 0);
        log_info(w->logger, "llegue a recibir %d", *cod_op);

        switch (*cod_op)
        {
        case STORAGE_SEND_BLOCK_SIZE:
            int* size = list_get(valores, 1);
            w->tamanio_bloque = *size;
            log_info(w->logger, "Tamaño de bloque recibido: %d", w->tamanio_bloque);
            list_destroy_and_destroy_elements(valores, free);
            break;
        
        default:
        log_info(w->logger, "Error en el cod_op %d", *cod_op);
            break;
        }
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

void retener_worker(t_worker* w){
    
    log_warning(w->logger, "Retengo al worker antes de crear el interrupt"); //tiene warning solo para que se diferencie

    t_list* paquete;
    while(paquete = recibir_paquete(w->master_socket_distpach)){
    int* cod_op =  list_get(paquete, 0);

    if (*cod_op == -1)
    {
        log_error(w->logger, "Error en la conexion con MAster");
        return NULL;
    }

    if (*cod_op == RETENER_WORKER)
    log_info(w->logger, "Pude pasar de retener worker");
    return NULL;

    }
    
}

void avisar_error_generico(t_worker* w, op_code etiqueta){ //solo porque son paquetes vacios 

    loggerError(w,etiqueta);
    /*
    t_buffer* buffer1 = crear_buffer();
    t_paquete* paqueteError = crear_paquete(etiqueta, buffer1);
    enviar_paquete(paqueteError, w->master_socket_distpach, w->logger);
    eliminar_paquete(paqueteError);
    */

}

//SOlo para ver si anda bien el envio 
void loggerError(t_worker* w, op_code etiqueta){
    switch (etiqueta)
    {
    case WORKER_ERROR_ARCHIVO:
        log_info(w->logger, "Envie a master el WORKER_ERROR_ARCHIVO");
        break;
    
    case WORKER_INSTRUCCION_MALFORMADA:
        log_info(w->logger, "Envie a master el WORKER_INSTRUCCION_MALFORMADA");
        break;
    

    default:
    log_info(w->logger, "default");
        break;
    }
}
