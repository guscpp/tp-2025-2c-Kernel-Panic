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
    w->puerto_master = config_get_string_value(w->config, "PUERTO_MASTER");
    w->ip_storage = config_get_string_value(w->config, "IP_STORAGE");
    w->puerto_storage = config_get_string_value(w->config, "PUERTO_STORAGE");
    w->path_scripts = config_get_string_value(w->config, "PATH_QUERIES");
    w->log_level = config_get_string_value(w->config, "LOG_LEVEL");
    w->id_worker = id_worker;
    w->master_socket = -1;
    w->storage_socket = -1;

    return w;
}

void verificar_worker(t_worker* w)
{
    log_info(w->logger, "Ip_Master: %s", w->ip_master);
    log_info(w->logger, "Puerto Master: %s", w->puerto_master);
    log_info(w->logger, "Ip_Storage: %s", w->ip_storage);
    log_info(w->logger, "Puerto_Storage: %s", w->puerto_storage);
    // log_info(w->logger, "Tam_memoria: %d", w->tamanio_memoria);
    // log_info(w->logger, "Retardo_Memoria: %d", w->retardo_memoria);
    // log_info(w->logger, "Algoritmos_reemplazo: %s", w->algoritmo_reemplazo);
    log_info(w->logger, "PathScript: %s", w->path_scripts);
}

void procesar_asignacion_query(int master_socket, t_log* logger)
{
    while (1)
    {
        int codigo_operacion = recibir_operacion(master_socket);
        
        if (codigo_operacion == -1) {
            log_error(logger, "Error en la conexión con el Master");
            break;
        }
        
        if (codigo_operacion == WORKER_ASSIGN_QUERY)
        {
            // Recibir información de la query
            int size_path;
            recv(master_socket, &size_path, sizeof(int), MSG_WAITALL);
            
            char* path_query = malloc(size_path);
            recv(master_socket, path_query, size_path, MSG_WAITALL);
            
            int prioridad;
            recv(master_socket, &prioridad, sizeof(int), MSG_WAITALL);
            
            log_info(logger, "Query asignada: %s (prioridad: %d)", path_query, prioridad);
            
            // TODO: Codigo para procesar la query
            
            free(path_query);
            break;
        }
    }
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

// Esta funcion puede adaptarse despues del checkpoint 1 para
// procesar queries, no solo recibir y mandar paths al log
//Podria devolver el path o directamente el archivo

/*
void recibir_path_de_query(int master_socket, t_log* logger)
{
    int codigo_operacion = recibir_operacion(master_socket);

    if (codigo_operacion == -1)
    {
        log_error(logger, "Error en la conexión con el Master");
        return;
    }

    if (codigo_operacion == WORKER_ASSIGN_QUERY)
    {
        t_list* valores = recibir_paquete(master_socket);
        
        if (valores && list_size(valores) >= 3)
        {
            // Los valores vienen en el orden:
            // query_id, path_query, prioridad
            int query_id = *(int*)list_get(valores, 0);
            char* path_query = (char*)list_get(valores, 1);
            int prioridad = *(int*)list_get(valores, 2);
            
            log_info(logger, "Query recibida: ID=%d, Path=%s, Prioridad=%d", 
                    query_id, path_query, prioridad);
            
            // Liberar la lista (pero no los elementos)
            list_destroy(valores);
            
            free(path_query);
        }
    }
}
*/

Pcb* recibir_path_de_query(int master_socket, t_worker* w) 
{
    log_info(w->logger, "Por lo menos entre a recibir_path");

    log_info(w->logger, "ESpero que me asignen queries");
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
        int tamanio_valores = list_size(paquete_path); 
        if (paquete_path && list_size(paquete_path)>= 3) 
        {
            // Los valores vienen en el orden:
            // query_id, path_query, prioridad

            //Por el error de utils lo comento y hardcodeo valores
            int query_id = *(int*)list_get(paquete_path, 1);
            char* path_query = (char*)list_get(paquete_path, 2);
            int prioridad = *(int*)list_get(paquete_path, 3); //(*)

            int pc = 0; //pongo este porque en el codigo master todavia no me manda el pc
            
            /*ESTOS SON LOS VALORES DE PRUEBA QUE ANDAN;  //EStos eran los valores hardcodeados de antes porque no funcionaba el pasaje de datos entre master y worker
           //SOLO PARA PROBARLO
           int query_id = 2;
           char* path_query = "prueba.txt";
           int prioridad = 1;
           int pc = 0; //PARA UN PROCESO SIN INTERRUPCION
           //int pc = 4;
            */

            log_info(w->logger, "Query recibida: ID=%d, Path=%s, Prioridad=%d, PC:%d", 
                    query_id, path_query, prioridad, pc);  

            if(pc > 0){
                log_info(w->logger, "Es un proceso que fue interrumpido antes");
            }
            
            dt_archivo = malloc(sizeof(Pcb));
            dt_archivo->nombre_archivo = path_query;
            dt_archivo->query_id = query_id;
            dt_archivo->archivo = retornar_archivo(path_query, w->path_scripts, w->logger);
            dt_archivo->pc = pc;
           
            // Liberar la lista (pero no los elementos)
            
            list_destroy(paquete_path);
            return dt_archivo;

            
            //free(path_query); //?
        }
    }
    return dt_archivo;
}


FILE* retornar_archivo(char* nombre_archivo, char* path_general, t_log* logger){ //esto listo

    char* path_final = string_new();
    string_append(&path_final, path_general);
    string_append(&path_final, nombre_archivo);
    
    FILE* archivo_query = fopen(path_final, "r"); //EN ESTE CASO TENGO QUE PONERLO ASI PORQUE LO PRUEBO DESDE WORKER/

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

    t_ejecucion* datos_ejecucion = (t_ejecucion*) arg;

    log_info(datos_ejecucion->w->logger, "Por lo menos entre a ejecutar_query");
    Pcb* dt_archivo;

    while(dt_archivo = recibir_path_de_query(datos_ejecucion->master_socket, datos_ejecucion->w)){ //retorna el pcb con los datos del proceso a ejecutar

        log_info(datos_ejecucion->w->logger, "Llego el path_query: %s", dt_archivo->nombre_archivo);
        if(dt_archivo == NULL){
            log_info(datos_ejecucion->w->logger, "Error al abrir query, estoy en worker.c");
            return;
        }
        query_interpreter_ciclo(dt_archivo, datos_ejecucion->w); 
    
    }
}
//-------------------------------------------------------------------------------------------------


void rtas_storage(int storage_socket, t_worker* w){
        t_list* valores = recibir_paquete(storage_socket);
        int* cod_op = list_get(valores, 0);
        log_info(w->logger, "llegue a recibir %d", *cod_op);

        switch (*cod_op)
        {
        case STORAGE_SEND_BLOCK_SIZE:
            
            int* size = list_get(valores, 1);
            log_info(w->logger, "Rta tamanio de bloque: %d", *size);
            //cuidado de free
            break;
        
        default:
        log_info(w->logger, "Error en el cod_op %d", *cod_op);
            break;
        }
}

//------------------HILO DE ATENCION DE INTERRUPCIONES-----------------------
void* hilo_atender_interrupcion(void* arg){ //Cuando me lleguen interrupciones, las guardo en el interpreter que tiene un booleano encargado de chquear eso. Y en el ciclo siempre revisamos ese bool
    
    t_ejecucion* dt_atender_master = (t_ejecucion*) arg; 
    log_info(dt_atender_master->w->logger, "ENtre a hilo de interrupciones");
    //sleep(2);   

    dt_atender_master->w->interpreter->hay_interrupcion = false;
    //SOLO PARA PROBAR QUE FUNCIONEN LAS INTERRUPCIONES: quiero que dsp de 15 segundos me mande una interrupcion
    //sleep(15); //prueba
    
    /*
    while(recibir_interrupciones(dt_atender_master->master_socket, dt_atender_master->w)){//solo devuelve true si es cierto
        dt_atender_master->w->interpreter->hay_interrupcion = true; //aca marcamos en true la interrupcion para verificarlo despues en el ciclo de instrucciones
        
    }
    */
}
//-------------------------------------------------------------------------------------------------

bool recibir_interrupciones(int master_socket, t_worker* w){ //SOlo se encarga de devolver true en el caso de que llegue una interrupcion


    t_list* paquete_interrupcion = recibir_paquete(master_socket);
    int* codigo_operacion =  list_get(paquete_interrupcion, 0);

    if (codigo_operacion == -1)
    {
        log_error(w->logger, "Error en la conexión con el Master");
        return false;
    }

    if (codigo_operacion == WORKER_DESALOJO)
    {
        t_list* valores = recibir_paquete(master_socket);
        log_info(w->logger, "Llegue a recibir el paquete interrupcion de MAster");

        if (valores && list_size(valores) == 1)
        {
            
            log_info(w->logger, "Me llego una interrupcion");
            
            list_destroy(valores);
            return true;

        }
    }
    return false;
}