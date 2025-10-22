#include "master.h"
#include <unistd.h>
#include <stdlib.h>
// VER ID WORKERRRRR
// CONTADORES
int cantidadWorkers = 0;
int idQuery = 0;
// MUTEXX
pthread_mutex_t mutexCantWorkers;
pthread_mutex_t mutexIdQuery;
pthread_mutex_t mutexColaQuery;
pthread_mutex_t mutexQueryEnWorker;

// COLA Y SEMAFORO COLA
sem_t sem_queries;
t_list* cola_queries;
t_list* query_en_worker;


void inicializar_semaforos(t_log* logger){
    if(pthread_mutex_init(&mutexCantWorkers,NULL) != 0){
        log_warning(logger,"Error al inicializar el mutex cantWorkers");

    }

    if(pthread_mutex_init(&mutexIdQuery,NULL) != 0){
        log_warning(logger,"Error al inicializar el mutex idQuery");
    }
    
    cola_queries = list_create();
    if (cola_queries == NULL) {
        log_warning(logger, "Error al crear la lista de queries");
        
    }

    query_en_worker = list_create();
    if (query_en_worker == NULL) {
        log_warning(logger, "Error al crear la lista query_en_worker");
    }

    if (sem_init(&sem_queries, 0, 0) != 0) {
        log_warning(logger, "Error al inicializar el semáforo de queries");
       
    }

    if(pthread_mutex_init(&mutexColaQuery,NULL) != 0){
        log_warning(logger,"Error al inicializar el mutex ColaQuery");
    }
    if(pthread_mutex_init(&mutexQueryEnWorker,NULL) != 0){
        log_warning(logger,"Error al inicializar el mutex QueryEnWorker");
    }

    log_info(logger, "Semáforos y mutex inicializados correctamente");
}


void* atender_conexion(void* arg){
    
    t_hacerConnect* informacion = (t_hacerConnect*) arg;

    t_list* paqueteHandshake = recibir_paquete(informacion->socket_conexion);
    int* handshake = list_get(paqueteHandshake, 0);
    switch(*handshake){
        case QC_HANDSHAKE:
        log_info(informacion->logger, "Se ha conectado un Query");
        list_destroy_and_destroy_elements(paqueteHandshake, free);
        atender_Query(informacion);
        
        break;
        
        case WORKER_HANDSHAKE:
        log_info(informacion->logger, "Se ha conectado un worker");
        list_destroy_and_destroy_elements(paqueteHandshake, free);
        atender_Worker(informacion);
        break;
        
        default:
        log_warning(informacion->logger,"Operacion desconocida");
        list_destroy_and_destroy_elements(paqueteHandshake, free);
		break;

    }

   // close(informacion->socket_conexion);
    //free(informacion);
   return NULL;
}


void atender_Query(t_hacerConnect*  informacion){
   t_list* paqueteQuery = recibir_paquete(informacion->socket_conexion);
   
   pthread_t hilo_vigilante;
   pthread_create(&hilo_vigilante, NULL, atender_desconexion_query, informacion);
   pthread_detach(hilo_vigilante);
   

   int* codOperacion = list_get(paqueteQuery, 0);

   if (*codOperacion != QUERY_REQUEST){
        log_warning(informacion->logger,"Operacion desconocida");
   }

   t_query* nuevaQuery = malloc(sizeof(t_query));

   asignar_id_query(&nuevaQuery->id);
   informacion->id = nuevaQuery->id;
   char* pathFromPaquete = list_get(paqueteQuery, 1);
   nuevaQuery->path = string_duplicate(pathFromPaquete);
   nuevaQuery->prioridad = *(int*)list_get(paqueteQuery, 2);

   list_destroy_and_destroy_elements(paqueteQuery, free);

   nuevaQuery->socket = informacion->socket_conexion;
   nuevaQuery->programCounter = 0 ;
   nuevaQuery->estado= READY;
   
   log_info(informacion->logger, "Se conecta un Query Control para ejecutar la Query %s con prioridad %d - Id asignado: %d ", nuevaQuery->path, nuevaQuery->prioridad, nuevaQuery->id);


   pthread_mutex_lock(&mutexColaQuery);
   list_add(cola_queries, nuevaQuery);
   pthread_mutex_unlock(&mutexColaQuery);
 // SOLO PARA PRUEBASSS
    char* idsEnCola = string_new(); // string_new() de commons, crea string vacío

    for (int i = 0; i < list_size(cola_queries); i++) {
        t_query* q = list_get(cola_queries, i);
        string_append_with_format(&idsEnCola, "%d ", q->id);
    }
// PRUEBAAAAA
    log_info(informacion->logger, "se agrego query a la cola, cola actual:  %s", idsEnCola);

    // comento esto abajo porque esta desconectando al QC
    t_buffer* infoQuery = crear_buffer();
    t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_READ, infoQuery);
    enviar_paquete( paquete,  informacion->socket_conexion ,  informacion->logger);
    // close(informacion->socket_conexioatender_Queryn );
    
    sem_post(&sem_queries);

}

void asignar_id_query(int* idAsignado){
   
    pthread_mutex_lock(&mutexIdQuery);
    *idAsignado = idQuery ++; 
    pthread_mutex_unlock(&mutexIdQuery);
    
}


void* atender_desconexion_query(void* arg){
    t_hacerConnect* informacion = (t_hacerConnect*) arg;
  
    char buffer[1];

    int ret = recv(informacion->socket_conexion, buffer, 1, 0);

    if (ret == 0) {
      
    
        log_warning(informacion->logger, "QUERY SE DESCONECTO ID: %d", informacion->id);
        close(informacion->socket_conexion);
        free(informacion);
        } 
       

    return NULL;

}


void atender_Worker(t_hacerConnect* informacion){
    t_list* paqueteWorker = recibir_paquete(informacion->socket_conexion);
  
        if (paqueteWorker == NULL) {
        log_warning(informacion->logger, "WORKER SE DESCONECTO ID: %d", informacion->id);
        close(informacion->socket_conexion);
        free(informacion);
        return;
        } 

    int* codOperacion =  list_get(paqueteWorker, 0);
    switch (*codOperacion){
        case WORKER_ID:{
            
            
            int* idWorkerLista = list_get(paqueteWorker, 1);
            int idWorker = *idWorkerLista ;
            informacion->id = idWorker;
            pthread_mutex_lock(&mutexCantWorkers);
            cantidadWorkers ++;
            log_info(informacion->logger, "Se ha conectado un worker  ID: %d  CANTIDAD TOTAL DE WORKERS: %d", idWorker , cantidadWorkers);
            
            pthread_mutex_unlock(&mutexCantWorkers);
            list_destroy_and_destroy_elements(paqueteWorker, free);
             
            comenzar_a_ejecutar(informacion,idWorker);
           
            break;
        }
        case   WORKER_READ_RESULT:{
            
               t_query* queryRecivida;
               t_readQuery* readQuery = malloc(sizeof(t_readQuery));

               int idQueryLista = *(int*)list_get(paqueteWorker, 1);

               pthread_mutex_lock(&mutexQueryEnWorker);
               queryRecivida = obtener_por_id(query_en_worker, idQueryLista);
               pthread_mutex_unlock(&mutexQueryEnWorker);
               
               readQuery->contenido = list_get(paqueteWorker, 2);
               readQuery->file =  list_get(paqueteWorker, 3);
               readQuery->tag = list_get(paqueteWorker, 4);
               queryRecivida->programCounter = *(int*)list_get(paqueteWorker, 5);
                   
               log_info(informacion->logger,"cargando paquete; contenido: %s, file:  %s, tag:  %s", readQuery->contenido,readQuery->file, readQuery->tag );
               enviar_read_a_query(queryRecivida, readQuery,informacion);
               

               list_destroy_and_destroy_elements(paqueteWorker, free);
            
               atender_Worker(informacion);
            break;
        }
        case WORKER_QUERY_END:{

            printf("///// Entra a case woeker_query_end  \n");
            t_query* queryTerminada;
            
            printf("/////");

            int idQueryLista = *(int*)list_get(paqueteWorker, 1);
            
            printf("/////");

            pthread_mutex_lock(&mutexQueryEnWorker);
            queryTerminada = eliminar_por_id(query_en_worker, idQueryLista);
            pthread_mutex_unlock(&mutexQueryEnWorker);
           
            int idWorker = queryTerminada -> idWorker;

            printf("/////");

            query_completado_con_exito( queryTerminada , informacion );

            printf("/////");

            list_destroy_and_destroy_elements(paqueteWorker, free);
            
            printf("/////");
            
            comenzar_a_ejecutar(informacion,idWorker);

            break;
        } 
        default:{
            log_warning(informacion->logger, "Operacion desconocida");
            break;
        } 
    }
}
void   enviar_read_a_query(t_query* queryRecivida, t_readQuery* readQuery,t_hacerConnect* informacion ){
    t_buffer* lecturaQuery = crear_buffer();

    t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_READ, lecturaQuery);

    agregar_a_paquete(paquete,lecturaQuery,strlen(readQuery->contenido) + 1);
    agregar_a_paquete(paquete,lecturaQuery,strlen(readQuery->file) + 1);
    agregar_a_paquete(paquete,lecturaQuery,strlen(readQuery->tag) + 1);

    enviar_paquete(paquete, queryRecivida->socket, informacion->logger);
    eliminar_paquete(paquete);


}
void query_completado_con_exito(t_query* query,t_hacerConnect* informacion ){
    t_buffer* infoQuery = crear_buffer();

    t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_END, infoQuery);
    char* motivo= "holaa";
    agregar_a_paquete(paquete,motivo,strlen(motivo) + 1);

    enviar_paquete( paquete,  query->socket ,  informacion->logger);
    log_info(informacion->logger, "Query id: %d terminada con exito", query->id);
    close(query->socket);
    log_info(informacion->logger, "Comunicacion Cerrada con Query");
    eliminar_paquete(paquete);
    free(query);

}
void comenzar_a_ejecutar(t_hacerConnect* informacion, int idWorker){
    sem_wait(&sem_queries);

    pthread_mutex_lock(&mutexColaQuery);

    t_query* query = obtener_menor_prioridad(cola_queries);
    
      // SOLO PARA PRUEBASSS
    char* idsEnCola = string_new(); // string_new() de commons, crea string vacío
    query->estado= RUNNING;
    for (int i = 0; i < list_size(cola_queries); i++) {
    t_query* q = list_get(cola_queries, i);
    string_append_with_format(&idsEnCola, "%d ", q->id);
}
// PRUEBAAAAA
    log_info(informacion->logger, "se quito query a la cola, cola actual:  %s", idsEnCola);
    pthread_mutex_unlock(&mutexColaQuery);

    pthread_mutex_lock(&mutexQueryEnWorker);
    query->idWorker = idWorker;
    list_add(query_en_worker, query);
    pthread_mutex_unlock(&mutexQueryEnWorker);



    enviar_query_a_worker(query,informacion, idWorker );
}

void enviar_query_a_worker(t_query* query,t_hacerConnect* informacion, int idWorker){
    t_buffer* infoQuery = crear_buffer();

    t_paquete* paquete  = crear_paquete(WORKER_ASSIGN_QUERY, infoQuery);

    agregar_a_paquete(paquete,&query->id,sizeof(int));
    agregar_a_paquete(paquete,query->path,strlen(query->path) + 1);
    agregar_a_paquete(paquete,&query->prioridad,sizeof(int));
    agregar_a_paquete(paquete,&query->programCounter,sizeof(int));
    

    int desconexion = enviar_paquete( paquete,  informacion->socket_conexion ,  informacion->logger);
    if (desconexion == -1){
        log_warning(informacion->logger, "WORKER SE DESCONECTO ID: %d", informacion->id);
        close(informacion->socket_conexion);
        free(informacion);
    }

    log_info(informacion->logger, "se ha enviado al worker id: %d una query", idWorker);

    eliminar_paquete(paquete);

    atender_Worker(informacion);

}

t_query* eliminar_por_id(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_query* query) {
        return query->id == idBuscado;
    }

    return list_remove_by_condition(lista, (void*) coincide_id);
}

t_query* obtener_por_id(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_query* query) {
        return query->id == idBuscado;
    }

    return list_find(lista, (void*) coincide_id);
}

t_query* obtener_menor_prioridad(t_list* lista_queries) {
    if (list_is_empty(lista_queries))
        return NULL;

    int min_index = 0;
    t_query* min_query = list_get(lista_queries, 0);

    for (int i = 1; i < list_size(lista_queries); i++) {
        t_query* actual = list_get(lista_queries, i);
        if (actual->prioridad < min_query->prioridad) {
            min_query = actual;
            min_index = i;
        }
    }

    // Lo saca de la lista y lo devuelve (sin liberar)
    return list_remove(lista_queries, min_index);
}
