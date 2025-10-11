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
   char* pathFromPaquete = list_get(paqueteQuery, 1);
   nuevaQuery->path = string_duplicate(pathFromPaquete);
   nuevaQuery->prioridad = *(int*)list_get(paqueteQuery, 2);

   list_destroy_and_destroy_elements(paqueteQuery, free);

   nuevaQuery->socket = informacion->socket_conexion;
   nuevaQuery->programCounter = 0 ;
   
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
    printf("//////");
    t_buffer* infoQuery = crear_buffer();
    printf("//////");
    t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_END, infoQuery);
    printf("//////");
    enviar_paquete( paquete,  informacion->socket_conexion ,  informacion->logger);
    printf("//////");
    //close(informacion->socket_conexion );
    
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
      
    
        log_warning(informacion->logger, "QUERY SE DESCONECTO");
        close(informacion->socket_conexion);
        free(informacion);
        } 
       

    return NULL;

}


void atender_Worker(t_hacerConnect* informacion){
    t_list* paqueteWorker = recibir_paquete(informacion->socket_conexion);
    int* codOperacion =  list_get(paqueteWorker, 0);
    switch (*codOperacion){
        case WORKER_ID:{
            
            
            int* idWorkerLista = list_get(paqueteWorker, 1);
            int idWorker = *idWorkerLista ;

            pthread_mutex_lock(&mutexCantWorkers);
            cantidadWorkers ++;
            log_info(informacion->logger, "Se ha conectado un worker  ID: %d  CANTIDAD TOTAL DE WORKERS: %d", idWorker , cantidadWorkers);
            
            pthread_mutex_unlock(&mutexCantWorkers);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            comenzar_a_ejecutar(informacion, idWorker);
           
            break;
        }
        case   WORKER_READ_RESULT:{
               /*t_query* queryRecivida;
               int idQueryLista = *(int*)list_get(paqueteWorker, 1);

               pthread_mutex_lock(&mutexQueryEnWorker);
               queryRecivida = eliminar_por_id(query_en_worker, idQueryLista);
               pthread_mutex_unlock(&mutexQueryEnWorker);
               
               char* pathFromPaquete = list_get(paqueteWorker, 2);
               queryRecivida->path = string_duplicate(pathFromPaquete);
               queryRecivida->prioridad = *(int*)list_get(paqueteWorker, 3);
               
               list_destroy_and_destroy_elements(paqueteWorker, free);
              */
            break;
        }
        case WORKER_QUERY_END:{
            t_query* queryTerminada;
            int idQueryLista = *(int*)list_get(paqueteWorker, 1);
          

            pthread_mutex_lock(&mutexQueryEnWorker);
            queryTerminada = eliminar_por_id(query_en_worker, idQueryLista);
            pthread_mutex_unlock(&mutexQueryEnWorker);
           
            int idWorker = queryTerminada -> idWorker;
            
            query_completado_con_exito( queryTerminada , informacion );

            list_destroy_and_destroy_elements(paqueteWorker, free);
            
            comenzar_a_ejecutar(informacion,idWorker);

            break;
        } 
        default:{
            log_warning(informacion->logger, "Operacion desconocida");
            break;
        } 
    }
}
void query_completado_con_exito(t_query* query,t_hacerConnect* informacion ){
    t_buffer* infoQuery = crear_buffer();

    t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_END, infoQuery);

    enviar_paquete( paquete,  query->socket ,  informacion->logger);
    close(query->socket);
    
    eliminar_paquete(paquete);
    
    free(query);

}
void comenzar_a_ejecutar(t_hacerConnect* informacion, int idWorker){
    sem_wait(&sem_queries);

    pthread_mutex_lock(&mutexColaQuery);

    t_query* query = list_remove(cola_queries, 0);
    
      // SOLO PARA PRUEBASSS
   char* idsEnCola = string_new(); // string_new() de commons, crea string vacío

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
    

    enviar_paquete( paquete,  informacion->socket_conexion ,  informacion->logger);

    log_info(informacion->logger, "se ha enviado al worker id: %d una query", idWorker);

    eliminar_paquete(paquete);

    //atender_Worker(informacion);

}
t_query* eliminar_por_id(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_query* query) {
        return query->id == idBuscado;
    }

    return list_remove_by_condition(lista, (void*) coincide_id);
}

            



 
            



