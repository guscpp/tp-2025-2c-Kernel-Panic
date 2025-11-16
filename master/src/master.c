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
pthread_mutex_t mutexListaWorkers;

extern char* algoritmo_planificacion;
extern  int tiempo_aging;
// COLA Y SEMAFORO COLA
sem_t sem_queries;
sem_t sem_worker_cargado;
t_list* cola_queries;
t_list* query_en_worker;
t_list* lista_workers; // MUTEX ALREDEDOR DE LA LISTA 


void inicializar_semaforos(t_log* logger){
    if(pthread_mutex_init(&mutexCantWorkers,NULL) != 0){
        log_warning(logger,"Error al inicializar el mutex cantWorkers");

    }

    if(pthread_mutex_init(&mutexIdQuery,NULL) != 0){
        log_warning(logger,"Error al inicializar el mutex idQuery");
    }

    if(pthread_mutex_init(&mutexListaWorkers,NULL) != 0){
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
    if (sem_init(&sem_worker_cargado, 0, 0) != 0) {
        log_warning(logger, "Error al inicializar el semáforo de workers");
       
    }

    lista_workers= list_create();
    if (lista_workers == NULL) {
        log_warning(logger, "Error al crear la lista lista_workers");
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
        case WORKER_ID_INTERRUPT:
        log_info(informacion->logger, "Se ha conectado un worker interruption");
        int idWorker = *(int*)list_get(paqueteHandshake,1);
        list_destroy_and_destroy_elements(paqueteHandshake, free);
        atender_worker_interrupt(informacion,idWorker);
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

void atender_worker_interrupt(t_hacerConnect*  informacion,int id){
    pthread_mutex_lock(&mutexListaWorkers);
    t_worker* interrupt =  obtener_por_id_worker( lista_workers, id);
    interrupt->socket_interruption = informacion->socket_conexion;
    pthread_mutex_unlock(&mutexListaWorkers);
    log_info(informacion->logger, "se ha conectado worker interrupt de worker id: %d", id);
    pthread_t hilo_vigilante;
    pthread_create(&hilo_vigilante, NULL, atender_desconexion_worker, interrupt);
    pthread_detach(hilo_vigilante);
}
void* atender_desconexion_worker(void* arg){
    t_worker* informacion = (t_worker*) arg;
  
    char buffer[1];

    int ret = recv(informacion->socket_interruption, buffer, 1, 0);

    if (ret == 0) {
      
        pthread_mutex_lock(&mutexCantWorkers);
        cantidadWorkers ++;
        pthread_mutex_unlock(&mutexCantWorkers);
        log_warning(informacion->logger, COLOR_VERDE "## WORKER SE DESCONECTO ID: %d" COLOR_VERDE, informacion->id);
        close(informacion->socket_interruption);
        close(informacion->socket);
        pthread_mutex_lock(&mutexQueryEnWorker);
        t_query* query = obtener_query_por_id_worker(query_en_worker, informacion->id);
        pthread_mutex_unlock(&mutexQueryEnWorker);
        if (query != NULL){
        query->estado=EXIT;
        t_buffer* infoQuery = crear_buffer();

        t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_ERROR_WORKER_DESCONECTADO, infoQuery);

        char* motivo= "Finalizacion Hardcodeada";
        agregar_a_paquete(paquete,motivo,strlen(motivo) + 1);

        enviar_paquete( paquete,  query->socket ,  informacion->logger);
        log_info(informacion->logger, COLOR_VERDE "## Se termino la Query id: %d (prioridad: %d ) por desconexion en el worker %d" COLOR_VERDE, query->id, query->prioridad , query->idWorker);
      
        log_info(informacion->logger, "Comunicacion Cerrada con Query");
        eliminar_paquete(paquete);
        close(query->socket);
        //free(query);
        }
        free(informacion);
        } 
       

    return NULL;

}
t_query* obtener_query_por_id_worker(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_query* query) {
        return query->idWorker == idBuscado;
    }

    return list_find(lista, (void*) coincide_id);}
t_worker* obtener_por_id_worker(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_worker* worker) {
        return worker->id == idBuscado;
    }

    return list_find(lista, (void*) coincide_id);
}
void atender_Query(t_hacerConnect*  informacion){ // RECORDAR CAMBIAR ESTRUCTURA DE LOS HILOS PARA CODIGO MAS LIMPIOOO
    printf("///// \n");
   t_list* paqueteQuery = recibir_paquete(informacion->socket_conexion);
    printf("///// \n");

    t_query* nuevaQuery = malloc(sizeof(t_query));

   
   

   int* codOperacion = list_get(paqueteQuery, 0);

   if (*codOperacion != QUERY_REQUEST){
        log_warning(informacion->logger,"Operacion desconocida");
   }


   asignar_id_query(&nuevaQuery->id);
   informacion->id = nuevaQuery->id;
   nuevaQuery->alive = true;
   char* pathFromPaquete = list_get(paqueteQuery, 1);
   nuevaQuery->path = string_duplicate(pathFromPaquete);
   nuevaQuery->prioridad = *(int*)list_get(paqueteQuery, 2);
   nuevaQuery->logger = informacion->logger;
   list_destroy_and_destroy_elements(paqueteQuery, free);
   pthread_t hilo_vigilante;
   pthread_create(&hilo_vigilante, NULL, atender_desconexion_query, nuevaQuery);
   pthread_detach(hilo_vigilante);
   nuevaQuery->socket = informacion->socket_conexion;
   nuevaQuery->programCounter = 0 ;
   nuevaQuery->estado= READY;
    printf("///// \n");
   pthread_mutex_lock(&mutexCantWorkers);
   log_info(informacion->logger, COLOR_VERDE "## Se conecta un Query Control para ejecutar la Query %s con prioridad %d - Id asignado: %d . Nivel de multiprocesamiento %d" COLOR_VERDE, nuevaQuery->path, nuevaQuery->prioridad, nuevaQuery->id, cantidadWorkers);
    pthread_mutex_unlock(&mutexCantWorkers);

   pthread_mutex_lock(&mutexColaQuery);
   list_add(cola_queries, nuevaQuery);
   pthread_mutex_unlock(&mutexColaQuery);
 if(strcmp(algoritmo_planificacion, "PRIORIDADES") == 0){
    pthread_create(&nuevaQuery->hilo_timer, NULL, atender_timer_query, nuevaQuery);
   
 }
   // SOLO PARA PRUEBASSS
    {
    char* idsEnCola = string_new(); // string_new() de commons, crea string vacío

    for (int i = 0; i < list_size(cola_queries); i++) {
        t_query* q = list_get(cola_queries, i);
        string_append_with_format(&idsEnCola, "%d ", q->id);
    }
     
    log_info(informacion->logger, "se agrego query a la cola, cola actual:  %s", idsEnCola);
    } printf("///////");
if(strcmp(algoritmo_planificacion, "PRIORIDADES") == 0){
    printf("Antes de desalojo \n");
    chequeador_desalojo(nuevaQuery->prioridad,informacion);
    printf("DEspues de desalojo \n");
   }
    
    
    sem_post(&sem_queries);

}
void* atender_timer_query(void* arg){
    t_hacerConnect* informacion = (t_hacerConnect*) arg;
    pthread_mutex_lock(&mutexColaQuery);
    t_query* query = obtener_por_id(cola_queries, informacion->id);
    pthread_mutex_unlock(&mutexColaQuery);
    while (1) {
        if (!query->alive)
            break; 
        sleep(tiempo_aging);
        
        pthread_mutex_lock(&mutexColaQuery);
        query = obtener_por_id(cola_queries, informacion->id);
        if(query->estado == READY && query->prioridad > 0){
           
            int prioridad_ant= query->prioridad;
            -- query->prioridad;
            log_info(informacion->logger,COLOR_VERDE "##<QUERY_ID: %d> Cambio de prioridad: <PRIORIDAD_ANTERIOR: %d > - <PRIORIDAD_NUEVA %d>"COLOR_VERDE,informacion->id,prioridad_ant,query->prioridad );
            chequeador_desalojo(query->prioridad,informacion);
          
        }
          pthread_mutex_unlock(&mutexColaQuery);

    }
    log_info(informacion->logger, "Finaliza hilo de aging para query %d", informacion->id);
    return NULL;
}
void chequeador_desalojo(int prioridad,t_hacerConnect* info){
    pthread_mutex_lock(&mutexQueryEnWorker);
    if(list_is_empty(query_en_worker)){
        printf("lista vacia \n");
    pthread_mutex_unlock(&mutexQueryEnWorker);
        return;
    }
    t_query* queryMayor =  list_get_maximum(query_en_worker, _max_prioridad);
    pthread_mutex_lock(&mutexCantWorkers);
    if(list_size(query_en_worker) == cantidadWorkers && prioridad< queryMayor->prioridad){
       realizar_desalojo( queryMayor->id,info->logger);
    }
    pthread_mutex_unlock(&mutexCantWorkers);
    pthread_mutex_unlock(&mutexQueryEnWorker);
 
}

void realizar_desalojo(int idQuery,t_log* logger){
 log_info(logger, "Se va realizar el desalojo de query id: %d", idQuery);
        t_buffer* buffer=crear_buffer();
        t_paquete* paquete = crear_paquete(WORKER_DESALOJO,buffer);
        int a2 = 5;
        agregar_a_paquete(paquete, &a2, sizeof(int));
        pthread_mutex_lock(&mutexListaWorkers);
        t_worker* worker =  obtener_por_id_worker( lista_workers, idQuery);
        enviar_paquete(paquete, worker->socket_interruption,logger );
        log_info(logger, "Se realiza el desalojo de la query en worker id: %d", worker->id);
        pthread_mutex_unlock(&mutexListaWorkers);
        eliminar_paquete(paquete);
}
void asignar_id_query(int* idAsignado){
   
    pthread_mutex_lock(&mutexIdQuery);
    *idAsignado = idQuery ++; 
    pthread_mutex_unlock(&mutexIdQuery);
    
}

void* _max_prioridad(void* a, void* b) {
    t_query* query_a = (t_query*) a;
    t_query* query_b = (t_query*) b;
    return query_a->prioridad >= query_b->prioridad ? query_a : query_b;
}

void* atender_desconexion_query(void* arg){
    t_query* informacion = (t_query*) arg;
  
    char buffer[1];

    int ret = recv(informacion->socket, buffer, 1, 0);

    if (ret == 0) {
      
    
        log_warning(informacion->logger, "QUERY SE DESCONECTO ID: %d", informacion->id);
        close(informacion->socket);
        informacion->alive = false;
        if(informacion->estado == RUNNING){
            realizar_desalojo(informacion->id,informacion->logger);
        }
        if(strcmp(algoritmo_planificacion, "PRIORIDADES") == 0){
        pthread_join(informacion->hilo_timer, NULL);}
        free(informacion);
        } 
       

    return NULL;

}


void atender_Worker(t_hacerConnect* informacion){
    //printf("atenderWOrker\n");
    t_list* paqueteWorker = recibir_paquete(informacion->socket_conexion);
  
        if (paqueteWorker == NULL) {
        log_warning(informacion->logger,   "SE TERMINA HILO WORKER ID: %d", informacion->id);
        //close(informacion->socket_conexion);
        //free(informacion);
        return;
        } 

    int* codOperacion =  list_get(paqueteWorker, 0);
    switch (*codOperacion){

        case WORKER_ID:{
            t_worker* nuevoWorker = malloc(sizeof(t_worker));
            int* idWorkerLista = list_get(paqueteWorker, 1);
            int idWorker = *idWorkerLista ;
            informacion->id = idWorker;
            nuevoWorker->id = idWorker;
            nuevoWorker->logger = informacion->logger;
            pthread_mutex_lock(&mutexCantWorkers);
           
            cantidadWorkers ++;
            log_info(informacion->logger, COLOR_VERDE "## Se conecta el worker ID: %d  CANTIDAD TOTAL DE WORKERS: %d" COLOR_VERDE, idWorker , cantidadWorkers);
            
            pthread_mutex_unlock(&mutexCantWorkers);

            nuevoWorker->socket= informacion->socket_conexion;
            
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //agreggar a lista
            pthread_mutex_lock(&mutexListaWorkers);
            list_add(lista_workers, nuevoWorker);
            pthread_mutex_unlock(&mutexListaWorkers);
           
            t_buffer* paqueteDesalojo = crear_buffer();

            t_paquete* paquete = crear_paquete(RETENER_WORKER,paqueteDesalojo);
            int a = 2;
            agregar_a_paquete(paquete, &a, sizeof(int));
            enviar_paquete(paquete, informacion->socket_conexion, informacion->logger);
            eliminar_paquete(paquete);
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
               free(readQuery);
               atender_Worker(informacion);
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
        case WORKER_ERROR_ARCHIVO: {

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* file = (char*)list_get(paqueteWorker, 2);
            char* tag  = (char*)list_get(paqueteWorker, 3);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* errorArchivo = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!errorArchivo) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = errorArchivo->idWorker;
            errorArchivo->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_RESPONSE_ERROR_ARCHIVO_NO_ENCONTRADO, infoQuery);

            agregar_a_paquete(paquete, file, strlen(file) + 1);
            agregar_a_paquete(paquete, tag, strlen(tag) + 1);

            enviar_paquete(paquete, errorArchivo->socket, informacion->logger);

            log_info(informacion->logger,
                COLOR_VERDE "## Query Finalizada - Error: Archivo %s:%s No Encontrado (Query ID: %d, Worker: %d)"
                COLOR_VERDE,
                file, tag, idQuery, idWorker);

            // Cerrar conexión con el Query
            close(errorArchivo->socket);
            log_info(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
            free(errorArchivo);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;
        }

        case WORKER_ERROR_INSTRUCCION_MALFORMADA:{

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* instruccion = (char*)list_get(paqueteWorker, 2);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* errorInstruccion = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!errorInstruccion) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = errorInstruccion->idWorker;
            errorInstruccion->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_RESPONSE_ERROR_ERROR_EN_INSTRUCCION, infoQuery);

            agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);

            enviar_paquete(paquete, errorInstruccion->socket, informacion->logger);

            log_info(informacion->logger,
                COLOR_VERDE "## Query Finalizada - Error: Instruccion: %s mal formada (Query ID: %d, Worker: %d)"
                COLOR_VERDE,
                instruccion, idQuery, idWorker);

            // Cerrar conexión con el Query
            close(errorInstruccion->socket);
            log_info(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
            free(errorInstruccion);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
        case WORKER_ERROR_MODIFICAR_COMMIT:{
            
             // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* file = (char*)list_get(paqueteWorker, 2);
            char* tag = (char*)list_get(paqueteWorker, 3);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* errorCommit = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!errorCommit) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = errorCommit->idWorker;
            errorCommit->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_RESPONSE_ERROR_MODIFICAR_COMMIT, infoQuery);

            agregar_a_paquete(paquete, file, strlen(file) + 1);
            agregar_a_paquete(paquete, tag, strlen(tag) + 1);

            enviar_paquete(paquete, errorCommit->socket, informacion->logger);

            log_info(informacion->logger,
                COLOR_VERDE "## Query Finalizada - Error: Se intento modificar el archivo %s:%s que se encuentra en estado de COMMITED (Query ID: %d, Worker: %d)"
                COLOR_VERDE,
                file, tag, idQuery, idWorker);

            // Cerrar conexión con el Query
            close(errorCommit->socket);
            log_info(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
            free(errorCommit);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
        case WORKER_ERROR_TAMANIO_ESCRITURA_EXCEDIDO:{
            
            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* file = (char*)list_get(paqueteWorker, 2);
            char* tag = (char*)list_get(paqueteWorker, 3);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* errorTamanioEscritura = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!errorTamanioEscritura) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = errorTamanioEscritura->idWorker;
            errorTamanioEscritura->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_RESPONSE_ERROR_TAMANIO_ESCRITURA_EXCEDIDO, infoQuery);

            agregar_a_paquete(paquete, file, strlen(file) + 1);
            agregar_a_paquete(paquete, tag, strlen(tag) + 1);

            enviar_paquete(paquete, errorTamanioEscritura->socket, informacion->logger);

            log_info(informacion->logger,
                COLOR_VERDE "## Query Finalizada - Error: Se Excedio el Tamaño de Escritura de el Archivo %s:%s (Query ID: %d, Worker: %d)"
                COLOR_VERDE,
                file, tag, idQuery, idWorker);

            // Cerrar conexión con el Query
            close(errorTamanioEscritura->socket);
            log_info(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
            free(errorTamanioEscritura);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;



        }
        case WORKER_ERROR_STORAGE_DESCONECTADO:{

             // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* errorStorageDesc = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!errorStorageDesc) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = errorStorageDesc->idWorker;
            errorStorageDesc->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_RESPONSE_ERROR_STORAGE_DESCONECTADO, infoQuery);


            enviar_paquete(paquete, errorStorageDesc->socket, informacion->logger);

            log_info(informacion->logger,
                COLOR_VERDE "## Query Finalizada - Error: Storage Desconectado (Query ID: %d, Worker: %d)"
                COLOR_VERDE,
                idQuery, idWorker);

            // Cerrar conexión con el Query
            close(errorStorageDesc->socket);
            log_info(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
            free(errorStorageDesc);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
        case WORKER_PC_UPDATE:{
            // tengo funcion obtener y eliminar por id... 
            t_query* queryRecivida;
            int idQuery = *(int*)list_get(paqueteWorker, 2);
           
            pthread_mutex_lock(&mutexQueryEnWorker);
            queryRecivida = eliminar_por_id(query_en_worker,idQuery );
            queryRecivida->estado= READY;
            pthread_mutex_unlock(&mutexQueryEnWorker);
             int idWorker = queryRecivida -> idWorker;
            queryRecivida->programCounter = *(int*)list_get(paqueteWorker, 3);
            pthread_mutex_lock(&mutexColaQuery);
            list_add(cola_queries, queryRecivida);
            pthread_mutex_unlock(&mutexColaQuery);
            log_info(informacion->logger,"se recivbio la query desalojada");
            comenzar_a_ejecutar(informacion,idWorker);

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

    agregar_a_paquete(paquete,readQuery->file,strlen(readQuery->file) + 1);
    agregar_a_paquete(paquete,readQuery->tag,strlen(readQuery->tag) + 1);
    agregar_a_paquete(paquete,readQuery->contenido,strlen(readQuery->contenido) + 1);

    enviar_paquete(paquete, queryRecivida->socket, informacion->logger);
    log_info(informacion->logger,"cargado paquete; contenido: %s, file:  %s, tag:  %s", readQuery->contenido,readQuery->file, readQuery->tag );

    eliminar_paquete(paquete);


}
void query_completado_con_exito(t_query* query,t_hacerConnect* informacion ){
    query->estado=EXIT;
    t_buffer* infoQuery = crear_buffer();

    t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_END, infoQuery);

    char* motivo= "Finalizacion Hardcodeada";
    agregar_a_paquete(paquete,motivo,strlen(motivo) + 1);

    enviar_paquete( paquete,  query->socket ,  informacion->logger);
    log_info(informacion->logger, COLOR_VERDE "## Se termino la Query id: %d (prioridad: %d )  con exito en el worker %d" COLOR_VERDE, query->id, query->prioridad , query->idWorker);
    close(query->socket);
    log_info(informacion->logger, "Comunicacion Cerrada con Query");
    eliminar_paquete(paquete);
    free(query);
 

}
void comenzar_a_ejecutar(t_hacerConnect* informacion, int idWorker){
    
    sem_wait(&sem_queries); // ESPERO A QUE HAYA UNA QUERY
    t_query* query;
    // MANDAR UN SEND DE COMO VOY A MANDARTE UNA QUERY PARA SABER SI ESTA VIVO
    pthread_mutex_lock(&mutexColaQuery);

    if(strcmp(algoritmo_planificacion, "FIFO") == 0){ // PLANIFICO SEGUN ALGORITMO
        query = list_remove(cola_queries, 0);
    }else{
        query = obtener_menor_prioridad(cola_queries);
    }
    
    
// SOLO PARA PRUEBASSS
    char* idsEnCola = string_new(); // string_new() de commons, crea string vacío
    
    for (int i = 0; i < list_size(cola_queries); i++) {
    t_query* q = list_get(cola_queries, i);
    string_append_with_format(&idsEnCola, "%d ", q->id);
    }
// PRUEBAAAAA

    log_info(informacion->logger, "se quito query a la cola, cola actual:  %s", idsEnCola);
    pthread_mutex_unlock(&mutexColaQuery);
    query->estado= RUNNING;

    pthread_mutex_lock(&mutexQueryEnWorker); 
    query->idWorker = idWorker;
    list_add(query_en_worker, query); // AGREGO QUERY A LISTA DE RUNNING
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
