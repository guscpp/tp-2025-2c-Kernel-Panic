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


// ----------------------------------------------------------------------------
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

    log_debug(logger, "Semáforos y mutex inicializados correctamente");
}


// ----------------------------------------------------------------------------
void* atender_conexion(void* arg){
    
    t_hacerConnect* informacion = (t_hacerConnect*) arg;

    t_list* paqueteHandshake = recibir_paquete(informacion->socket_conexion);
    int* handshake = list_get(paqueteHandshake, 0);
    switch(*handshake){
        case QC_HANDSHAKE:
        log_debug(informacion->logger, "Se ha conectado un Query");
        list_destroy_and_destroy_elements(paqueteHandshake, free);
        atender_Query(informacion);
        
        break;
        
        case WORKER_HANDSHAKE:
        log_debug(informacion->logger, "Se ha conectado un worker");
        list_destroy_and_destroy_elements(paqueteHandshake, free);
        atender_Worker(informacion);
        free(informacion);
        break;
        case WORKER_ID_INTERRUPT:
        log_debug(informacion->logger, "Se ha conectado un worker interruption");
        int idWorker = *(int*)list_get(paqueteHandshake,1);
        list_destroy_and_destroy_elements(paqueteHandshake, free);
        atender_worker_interrupt(informacion,idWorker);
        free(informacion);
        break;
        default:
        log_warning(informacion->logger,"Operacion desconocida");
        list_destroy_and_destroy_elements(paqueteHandshake, free);
		break;

    }

   // close(informacion->socket_conexion);
   
   return NULL;
}


// ----------------------------------------------------------------------------
void atender_worker_interrupt(t_hacerConnect*  informacion,int id){
    pthread_mutex_lock(&mutexListaWorkers);
    t_worker* interrupt =  obtener_por_id_worker( lista_workers, id);
    interrupt->socket_interruption = informacion->socket_conexion;
    pthread_mutex_unlock(&mutexListaWorkers);
    log_debug(informacion->logger, "se ha conectado worker interrupt de worker id: %d", id);
    pthread_t hilo_vigilante;
    pthread_create(&hilo_vigilante, NULL, atender_desconexion_worker, interrupt);
    pthread_detach(hilo_vigilante);
}


// ----------------------------------------------------------------------------
void* atender_desconexion_worker(void* arg){
    t_worker* informacion = (t_worker*) arg;
  
    char buffer[1];
    log_debug(informacion->logger, "holaa estoy antes del reciv");
    int ret = recv(informacion->socket_interruption, buffer, 1, 0);

    if (ret == 0) {
        informacion->desconection = true;
        close(informacion->socket_interruption);
        close(informacion->socket);
        pthread_mutex_lock(&mutexCantWorkers);
            cantidadWorkers --;
         pthread_mutex_unlock(&mutexCantWorkers);
        pthread_mutex_lock(&mutexQueryEnWorker);
        t_query* query = obtener_query_por_id_worker(query_en_worker, informacion->id);
        pthread_mutex_unlock(&mutexQueryEnWorker);
        
        if (query != NULL){
        query->estado=EXIT;
        
        t_buffer* infoQuery = crear_buffer();

        t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_ERROR_WORKER_DESCONECTADO, infoQuery);

        int idWorker= informacion->id;
        agregar_a_paquete(paquete,&idWorker,sizeof(int));

        enviar_paquete( paquete,  query->socket ,  informacion->logger);

        pthread_mutex_lock(&mutexCantWorkers);
       
        log_info(informacion->logger, "## Se desconecta el Worker %d - Se finaliza la Query %d - Cantidad total de Workers: %d ", informacion->id, query->id,cantidadWorkers);
        pthread_mutex_unlock(&mutexCantWorkers);

        
        eliminar_paquete(paquete);
        //close(query->socket);
        //free(query);
        }else{
            pthread_mutex_lock(&mutexCantWorkers);
            
            log_info(informacion->logger, "## Se desconecta el Worker %d - Cantidad total de Workers: %d ", informacion->id, cantidadWorkers);
            pthread_mutex_unlock(&mutexCantWorkers);
        }
        log_debug(informacion->logger, "Comunicacion Cerrada con Query");
        log_debug(informacion->logger, "espero a que se de cuenta q me mori");
        sem_wait(&informacion->desconexion_worker);
        log_debug(informacion->logger, "se dio cuenta");
        pthread_mutex_lock(&mutexListaWorkers);
        eliminar_worker_por_id( lista_workers,  informacion->id);
        pthread_mutex_unlock(&mutexListaWorkers);
        free(informacion);
        } 
       

    return NULL;

}


// ----------------------------------------------------------------------------
t_query* obtener_query_por_id_worker(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_query* query) {
        return query->idWorker == idBuscado;
    }

    return list_find(lista, (void*) coincide_id);}


// ----------------------------------------------------------------------------
t_worker* obtener_por_id_worker(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_worker* worker) {
        return worker->id == idBuscado;
    }

    return list_find(lista, (void*) coincide_id);
}


// ----------------------------------------------------------------------------
void atender_Query(t_hacerConnect*  informacion){ // RECORDAR CAMBIAR ESTRUCTURA DE LOS HILOS PARA CODIGO MAS LIMPIOOO
  
   t_list* paqueteQuery = recibir_paquete(informacion->socket_conexion);


    t_query* nuevaQuery = malloc(sizeof(t_query));

   
   t_log* logger = informacion->logger;

   int* codOperacion = list_get(paqueteQuery, 0);

   if (*codOperacion != QUERY_REQUEST){
        log_warning(logger,"Operacion desconocida");
   }


   asignar_id_query(&nuevaQuery->id);
   informacion->id = nuevaQuery->id;
   nuevaQuery->alive = true;
   char* pathFromPaquete = list_get(paqueteQuery, 1);
   nuevaQuery->path = string_duplicate(pathFromPaquete);
   nuevaQuery->prioridad = *(int*)list_get(paqueteQuery, 2);
   nuevaQuery->logger = logger;
   list_destroy_and_destroy_elements(paqueteQuery, free);
   pthread_t hilo_vigilante;
   pthread_create(&hilo_vigilante, NULL, atender_desconexion_query, nuevaQuery);
   pthread_detach(hilo_vigilante);
   nuevaQuery->socket = informacion->socket_conexion;
   nuevaQuery->programCounter = 0 ;
   nuevaQuery->estado= READY;
  
   pthread_mutex_lock(&mutexCantWorkers);
   log_info(logger, "## Se conecta un Query Control para ejecutar la Query %s con prioridad %d - Id asignado: %d . Nivel de multiprocesamiento %d" , nuevaQuery->path, nuevaQuery->prioridad, nuevaQuery->id, cantidadWorkers);
    pthread_mutex_unlock(&mutexCantWorkers);

   pthread_mutex_lock(&mutexColaQuery);
   list_add(cola_queries, nuevaQuery);
   pthread_mutex_unlock(&mutexColaQuery);
 if(strcmp(algoritmo_planificacion, "PRIORIDADES") == 0){
    if (sem_init(&nuevaQuery->timer_query, 0, 0) != 0) {
    log_debug(logger, "Error al inicializar el semáforo de queries");
    }
    // PASAMOS la nuevaQuery al hilo de timer (no 'informacion')
    if (pthread_create(&nuevaQuery->hilo_timer, NULL, atender_timer_query, nuevaQuery) != 0) {
        log_error(logger, "Error al crear hilo de timer para la query %d", nuevaQuery->id);
    }

    log_debug(logger,"Antes de desalojo \n");
    chequeador_desalojo(nuevaQuery->prioridad,logger);
   log_debug(logger,"DEspues de desalojo \n");
 }else{
    free(informacion);
 }
 /*
   // SOLO PARA PRUEBASSS
    {
    char* idsEnCola = string_new(); // string_new() de commons, crea string vacío

    for (int i = 0; i < list_size(cola_queries); i++) {
        t_query* q = list_get(cola_queries, i);
        string_append_with_format(&idsEnCola, "%d ", q->id);
    }
     
    log_info(informacion->logger, "se agrego query a la cola, cola actual:  %s", idsEnCola);
    } printf("///////");
     */

    
    
    sem_post(&sem_queries);
    log_debug(logger,"\033[35m Hago un sem_signal \033[0m\n");

}
// void* atender_timer_query(void* arg){
//     // Ahora arg es un t_query*
//     t_query* query = (t_query*) arg;
//     if (query == NULL) {
//         // proteccion defensiva
//         return NULL;
//     }

//     t_log* logger = query->logger;

//     // No hace falta buscar la query por id en la cola si ya tenemos el puntero.
//     while (1) {
//         // Si la query ya no está viva salimos
//         if (!query->alive) break;

//         sleep(tiempo_aging);

//         // Protegemos el acceso con mutex de cola
//         pthread_mutex_lock(&mutexColaQuery);
//         // Rechequeamos que la query siga en estado READY antes de modificar prioridad
//         if (query->estado == READY && query->prioridad > 0) {
//             int prioridad_ant = query->prioridad;
//             --query->prioridad;
//             log_info(logger,"## %d Cambio de prioridad: %d - %d", query->id, prioridad_ant, query->prioridad);
//             // chequeador_desalojo necesita un t_hacerConnect* (info). Tenés que adaptar llamada:
//             // actualmente chequeador_desalojo(query->prioridad, informacion);
//             // como no tenemos 'informacion' aquí, podemos pasar un respaldo o refactorizar chequeador_desalojo
//             // para que reciba solo prioridad y el id de la query. Para mantenerlo simple, llamamos con NULL y
//             // ajustamos chequeador_desalojo para tolerar NULL logger.
//             chequeador_desalojo(query->prioridad, logger);
//         }
//         pthread_mutex_unlock(&mutexColaQuery);

//         // Si la query está en otro estado esperamos a que la despierten (desalojo o desconexión)
//         if (query->estado != READY) {
//             // espera hasta que sem_post(&query->timer_query) ocurra (por desconexion o PC update)
//             sem_wait(&query->timer_query);
//         }

//         // loop continúa hasta que query->alive == false
//     }

//     log_debug(logger, "Finaliza hilo de aging para query %d", query->id);
//     // NO liberamos 'query' aquí — la liberación la maneja atender_desconexion_query
//     return NULL;
// }


// ----------------------------------------------------------------------------
void* atender_timer_query(void* arg){
    t_query* query = (t_query*) arg;
    if (query == NULL) return NULL;
    t_log* logger = query->logger;

    while (query->alive) {
        // CORRECCIÓN: Multiplicar por 1000 para pasar de ms a microsegundos
        usleep(tiempo_aging * 1000); 

        if (!query->alive) break;

        bool hubo_cambio = false;
        int prioridad_actualizada = 0;

        pthread_mutex_lock(&mutexColaQuery);
        // Verificar estado READY para aplicar aging
        if (query->estado == READY && query->prioridad > 0) {
            int prioridad_ant = query->prioridad;
            query->prioridad--;
            prioridad_actualizada = query->prioridad;
            hubo_cambio = true;
            log_info(logger,"## %d Cambio de prioridad: %d - %d", query->id, prioridad_ant, query->prioridad);
        }
        
        // Importante: Guardar el estado antes de soltar mutex
        int estado_actual = query->estado;
        pthread_mutex_unlock(&mutexColaQuery);

        // Llamar al chequeador SIN el mutex bloqueado para evitar el deadlock del punto 1
        if(hubo_cambio){
            chequeador_desalojo(prioridad_actualizada, logger);
        }

        // Si no está en READY, esperamos señal para dormir el hilo de aging
        if (estado_actual != READY && query->alive) {
            sem_wait(&query->timer_query);
        }
    }
    return NULL;
}


// ----------------------------------------------------------------------------
void chequeador_desalojo(int prioridad, t_log* logger)
{
   
    pthread_mutex_lock(&mutexColaQuery);
    pthread_mutex_lock(&mutexQueryEnWorker);
    if(list_is_empty(query_en_worker)){
        log_debug(logger,"lista vacia \n");
    pthread_mutex_unlock(&mutexQueryEnWorker);
    pthread_mutex_unlock(&mutexColaQuery);
        return;
    }

    t_query* queryMayor =  list_get_maximum(query_en_worker, _max_prioridad);
    pthread_mutex_lock(&mutexCantWorkers);
    if(list_size(query_en_worker) == cantidadWorkers && prioridad< queryMayor->prioridad){
       realizar_desalojo( queryMayor->id,queryMayor->prioridad,queryMayor->idWorker,logger,WORKER_DESALOJO);
    }
    pthread_mutex_unlock(&mutexCantWorkers);
    pthread_mutex_unlock(&mutexQueryEnWorker);
    pthread_mutex_unlock(&mutexColaQuery);

}


// ----------------------------------------------------------------------------
void realizar_desalojo(int idQuery, int prioridad, int idWorker,t_log* logger, int codOp){
 log_debug(logger, "Se va realizar el desalojo de query id: %d", idQuery);
        t_buffer* buffer=crear_buffer();
        t_paquete* paquete = crear_paquete(codOp,buffer);
        int a2 = 5;
        agregar_a_paquete(paquete, &a2, sizeof(int));
        pthread_mutex_lock(&mutexListaWorkers);
        t_worker* worker =  obtener_por_id_worker( lista_workers, idWorker );
        enviar_paquete(paquete, worker->socket_interruption,logger );
        if(codOp == WORKER_DESALOJO){
        log_info(logger," ## Se desaloja la Query <%d> (%d) del Worker %d  - Motivo:  PRIORIDAD",idQuery,prioridad,idWorker);
        }else{
        log_info(logger," ## Se desaloja la Query %d (%d) del Worker %d - Motivo:  DESCONEXION",idQuery,prioridad,idWorker); 
        }
        pthread_mutex_unlock(&mutexListaWorkers);
        eliminar_paquete(paquete);
}


// ----------------------------------------------------------------------------
void asignar_id_query(int* idAsignado){
   
    pthread_mutex_lock(&mutexIdQuery);
    *idAsignado = idQuery ++; 
    pthread_mutex_unlock(&mutexIdQuery);
    
}


// ----------------------------------------------------------------------------
void* _max_prioridad(void* a, void* b) {
    t_query* query_a = (t_query*) a;
    t_query* query_b = (t_query*) b;
    return query_a->prioridad >= query_b->prioridad ? query_a : query_b;
}


// ----------------------------------------------------------------------------
void* atender_desconexion_query(void* arg){
    t_query* informacion = (t_query*) arg;
  
    char buffer[1];
    int ret = recv(informacion->socket, buffer, 1, 0);
    if (ret == 0) {
         pthread_mutex_lock(&mutexCantWorkers);
        log_info(informacion->logger, "Se desconecta un Query Control. Se finaliza la Query %d con prioridad %d . Nivel multiprocesamiento %d ", informacion->id, informacion->prioridad, cantidadWorkers);
        pthread_mutex_unlock(&mutexCantWorkers);
        close(informacion->socket);
        informacion->alive = false;

        if(informacion->estado == RUNNING){
            realizar_desalojo(informacion->id, informacion->prioridad  ,informacion->idWorker, informacion->logger,WORKER_QUERY_DESCONECTADO );
        }
        if(informacion->estado == READY){
            pthread_mutex_lock(&mutexColaQuery);
            eliminar_por_id(cola_queries, informacion->id);
            pthread_mutex_unlock(&mutexColaQuery);
            sem_trywait(&sem_queries);

        }
        informacion->estado=EXIT;
        if (strcmp(algoritmo_planificacion, "PRIORIDADES") == 0) {
            sem_post(&informacion->timer_query);
            pthread_join(informacion->hilo_timer, NULL);
        }

        free(informacion);   // ← SOLO se libera aquí
        return NULL;
    }
    return NULL;
}


// ----------------------------------------------------------------------------
void atender_Worker(t_hacerConnect* informacion){
    //printf("atenderWOrker\n");
    t_list* paqueteWorker = recibir_paquete(informacion->socket_conexion);
  
        if (paqueteWorker == NULL) {
        log_debug(informacion->logger,   "SE TERMINA HILO WORKER ID: %d", informacion->id);
        //close(informacion->socket_conexion);
        //free(informacion);
        pthread_mutex_lock(&mutexListaWorkers);
        t_worker* worker= obtener_por_id_worker(lista_workers, informacion->id );
        pthread_mutex_unlock(&mutexListaWorkers);
        sem_post(&worker->desconexion_worker);
       // free(informacion);
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
            log_info(informacion->logger, "## Se conecta el worker  %d  -  Cantidad de Workers: %d" , idWorker , cantidadWorkers);
            
            pthread_mutex_unlock(&mutexCantWorkers);

            nuevoWorker->socket= informacion->socket_conexion;
            
            list_destroy_and_destroy_elements(paqueteWorker, free);
            nuevoWorker->desconection = false;
            //agreggar a lista
             if (sem_init(&nuevoWorker->desconexion_worker, 0, 0) != 0) {
             log_debug(informacion->logger, "Error al inicializar el semáforo de queries");
       
             }
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
                   
               log_debug(informacion->logger,"cargando paquete; contenido: %s, file:  %s, tag:  %s", readQuery->contenido,readQuery->file, readQuery->tag );
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
                log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , errorInstruccion->id, errorInstruccion->idWorker);

            log_debug(informacion->logger,
                 "## Query Finalizada - Error: Instruccion: %s mal formada (Query ID: %d, Worker: %d)"
                ,
                instruccion, idQuery, idWorker);

            // Cerrar conexión con el Query
            close(errorInstruccion->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorInstruccion);
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
            log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , errorCommit->id, errorCommit->idWorker);

            log_debug(informacion->logger,
                 "## Query Finalizada - Error: Se intento modificar el archivo %s:%s que se encuentra en estado de COMMITED (Query ID: %d, Worker: %d)"
                ,
                file, tag, idQuery, idWorker);

            // Cerrar conexión con el Query
            close(errorCommit->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorCommit);
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
            log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , errorTamanioEscritura->id, errorTamanioEscritura->idWorker);

            log_debug(informacion->logger,
                 "## Query Finalizada - Error: Se Excedio el Tamaño de Escritura de el Archivo %s:%s (Query ID: %d, Worker: %d)"
              ,
                file, tag, idQuery, idWorker);

            // Cerrar conexión con el Query
            close(errorTamanioEscritura->socket);
            log_info(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorTamanioEscritura);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;



        }
    

        // case WORKER_PC_UPDATE:{
        //     // tengo funcion obtener y eliminar por id... 
        //     t_query* queryRecivida;
        //     int idQuery = *(int*)list_get(paqueteWorker, 2);
           
        //     pthread_mutex_lock(&mutexQueryEnWorker);
        //     queryRecivida = eliminar_por_id(query_en_worker,idQuery );
        //     queryRecivida->estado= READY;
        //     pthread_mutex_unlock(&mutexQueryEnWorker);
        //     if(queryRecivida != NULL){
                
            
        //   //  int idWorker = queryRecivida -> idWorker;
        //     queryRecivida->programCounter = *(int*)list_get(paqueteWorker, 3);
        //     pthread_mutex_lock(&mutexColaQuery);
        //     list_add(cola_queries, queryRecivida);
        //     pthread_mutex_unlock(&mutexColaQuery);
        //     sem_post(&queryRecivida->timer_query);
        //     sem_post(&sem_queries);
        //     log_debug(informacion->logger,"\033[35m Hago un sem_signal \033[0m\n");
        //     log_debug(informacion->logger,"se recivbio la query desalojada");
        //     }
        //     comenzar_a_ejecutar(informacion,informacion->id);
        //     break;

        // }

        case WORKER_PC_UPDATE: {
            t_query* queryRecibida;
            int idQuery = *(int*)list_get(paqueteWorker, 2);

            // 1. Bloqueamos solo para sacar la query de la lista de ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            queryRecibida = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker); // <--- LIBERAMOS EL MUTEX AQUÍ (Clave para evitar Deadlock)

            if (queryRecibida != NULL) {
                queryRecibida->estado = READY;
                // Actualizamos el PC que viene del worker
                queryRecibida->programCounter = *(int*)list_get(paqueteWorker, 3);

                // 2. Ahora bloqueamos la cola para reinsertarla
                pthread_mutex_lock(&mutexColaQuery);
                list_add(cola_queries, queryRecibida);
                pthread_mutex_unlock(&mutexColaQuery);

                // Avisamos al semáforo de aging (si corresponde) y al general
                if (strcmp(algoritmo_planificacion, "PRIORIDADES") == 0) {
                     sem_post(&queryRecibida->timer_query);
                }
                sem_post(&sem_queries);
                
                log_debug(informacion->logger, "Se recibió la query desalojada y se encoló nuevamente");
            }
            
            list_destroy_and_destroy_elements(paqueteWorker, free);
            comenzar_a_ejecutar(informacion, informacion->id);
            break;
        }


         case WORKER_QUERY_DESCONECTADO:{
            // tengo funcion obtener y eliminar por id... 
            t_query* queryRecivida;
            int idQuery = *(int*)list_get(paqueteWorker, 2);
           
            pthread_mutex_lock(&mutexQueryEnWorker);
            queryRecivida = eliminar_por_id(query_en_worker,idQuery );
            queryRecivida->estado= EXIT;
            pthread_mutex_unlock(&mutexQueryEnWorker);
          
            log_debug(informacion->logger,"se recivbio la query desalojada por desconexion de query");
            
            comenzar_a_ejecutar(informacion,informacion->id);
            break;
        }
        case WORKER_ERROR_TAMANIO_LECTURA_EXCEDIDO:{
            
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
            t_paquete* paquete = crear_paquete(QUERY_RESPONSE_ERROR_TAMANIO_LECTURA_EXCEDIDO, infoQuery);

            agregar_a_paquete(paquete, file, strlen(file) + 1);
            agregar_a_paquete(paquete, tag, strlen(tag) + 1);

            enviar_paquete(paquete, errorTamanioEscritura->socket, informacion->logger);
            log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , errorTamanioEscritura->id, errorTamanioEscritura->idWorker);

            log_debug(informacion->logger,
                "## Query Finalizada - Error: Se Excedio el Tamaño de Escritura de el Archivo %s:%s (Query ID: %d, Worker: %d)",
                file, tag, idQuery, idWorker);

            // Cerrar conexión con el Query
            close(errorTamanioEscritura->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
            //free(errorTamanioEscritura);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;



        }
         case WORKER_ERROR_CREATE:{

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* instruccion = (char*)list_get(paqueteWorker, 2);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* query = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!query) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = query->idWorker;
            query->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_ERROR_CREATE, infoQuery);

            agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);

            enviar_paquete(paquete, query->socket, informacion->logger);
                log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , query->id, query->idWorker);


            // Cerrar conexión con el Query
            //close(query->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorInstruccion);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
        case WORKER_ERROR_TRUNCATE:{

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* instruccion = (char*)list_get(paqueteWorker, 2);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* query = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!query) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = query->idWorker;
            query->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_ERROR_TRUNCATE, infoQuery);

            agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);

            enviar_paquete(paquete, query->socket, informacion->logger);
                log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , query->id, query->idWorker);


            // Cerrar conexión con el Query
            //close(query->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorInstruccion);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
        case WORKER_ERROR_WRITE_EN_STORAGE:{

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* instruccion = (char*)list_get(paqueteWorker, 2);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* query = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!query) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = query->idWorker;
            query->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_ERROR_WRITE_EN_STORAGE, infoQuery);

            agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);

            enviar_paquete(paquete, query->socket, informacion->logger);
                log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , query->id, query->idWorker);


            // Cerrar conexión con el Query
           // close(query->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorInstruccion);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
         case WORKER_ERROR_READ_EN_STORAGE:{

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* instruccion = (char*)list_get(paqueteWorker, 2);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* query = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!query) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = query->idWorker;
            query->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_ERROR_READ_EN_STORAGE, infoQuery);

            agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);

            enviar_paquete(paquete, query->socket, informacion->logger);
                log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , query->id, query->idWorker);


            // Cerrar conexión con el Query
           // close(query->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorInstruccion);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
         case WORKER_ERROR_TAG:{

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* instruccion = (char*)list_get(paqueteWorker, 2);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* query = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!query) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = query->idWorker;
            query->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_ERROR_TAG, infoQuery);

            agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);

            enviar_paquete(paquete, query->socket, informacion->logger);
                log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , query->id, query->idWorker);


            // Cerrar conexión con el Query
           // close(query->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorInstruccion);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
        case WORKER_ERROR_COMMIT:{

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* instruccion = (char*)list_get(paqueteWorker, 2);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* query = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!query) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = query->idWorker;
            query->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_ERROR_COMMIT, infoQuery);

            agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);

            enviar_paquete(paquete, query->socket, informacion->logger);
                log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , query->id, query->idWorker);


            // Cerrar conexión con el Query
            //close(query->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorInstruccion);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
        case WORKER_ERROR_DELETE:{

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            char* instruccion = (char*)list_get(paqueteWorker, 2);

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* query = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!query) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = query->idWorker;
            query->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_ERROR_DELETE, infoQuery);

            agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);

            enviar_paquete(paquete, query->socket, informacion->logger);
                log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , query->id, query->idWorker);


            // Cerrar conexión con el Query
            //close(query->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorInstruccion);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
        case WORKER_ERROR_FLUSH:{

            // Datos del paquete recibido desde el Worker
            int idQuery = *(int*)list_get(paqueteWorker, 1);
            //char* instruccion = (char*)list_get(paqueteWorker, 2);
            char* instruccion = "Error flush";

            // Buscar la query en la lista de queries en ejecución
            pthread_mutex_lock(&mutexQueryEnWorker);
            t_query* query = eliminar_por_id(query_en_worker, idQuery);
            pthread_mutex_unlock(&mutexQueryEnWorker);

            if (!query) {
                log_warning(informacion->logger, "No se encontró la query %d en ejecución", idQuery);
                list_destroy_and_destroy_elements(paqueteWorker, free);
                break;
            }

            int idWorker = query->idWorker;
            query->estado = EXIT;

            // respuesta al Query Control
            t_buffer* infoQuery = crear_buffer();
            t_paquete* paquete = crear_paquete(QUERY_ERROR_FLUSH, infoQuery);

            agregar_a_paquete(paquete, instruccion, strlen(instruccion) + 1);

            enviar_paquete(paquete, query->socket, informacion->logger);
                log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , query->id, query->idWorker);


            // Cerrar conexión con el Query
            //close(query->socket);
            log_debug(informacion->logger, "Conexión cerrada con Query %d", idQuery);

            eliminar_paquete(paquete);
           // free(errorInstruccion);
            list_destroy_and_destroy_elements(paqueteWorker, free);

            //reiniciar Worker
            comenzar_a_ejecutar(informacion, idWorker);
            break;


        }
        default:{
            log_warning(informacion->logger, "Operacion desconocida");
         
            break;
        }
        return;
         
    }
}


// ----------------------------------------------------------------------------
void   enviar_read_a_query(t_query* queryRecivida, t_readQuery* readQuery,t_hacerConnect* informacion ){
    t_buffer* lecturaQuery = crear_buffer();

    t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_READ, lecturaQuery);

    agregar_a_paquete(paquete,readQuery->file,strlen(readQuery->file) + 1);
    agregar_a_paquete(paquete,readQuery->tag,strlen(readQuery->tag) + 1);
    agregar_a_paquete(paquete,readQuery->contenido,strlen(readQuery->contenido) + 1);

    enviar_paquete(paquete, queryRecivida->socket, informacion->logger);
    log_debug(informacion->logger,"cargado paquete; contenido: %s, file:  %s, tag:  %s", readQuery->contenido,readQuery->file, readQuery->tag );

    eliminar_paquete(paquete);


}


// ----------------------------------------------------------------------------
void query_completado_con_exito(t_query* query,t_hacerConnect* informacion ){
    query->estado=EXIT;
    t_buffer* infoQuery = crear_buffer();

    t_paquete* paquete  = crear_paquete( QUERY_RESPONSE_END, infoQuery);

    char* motivo= "Finalizacion Hardcodeada";
    agregar_a_paquete(paquete,motivo,strlen(motivo) + 1);

    enviar_paquete( paquete,  query->socket ,  informacion->logger);
    log_info(informacion->logger, "## Se termino la Query  %d  en el Worker %d" , query->id, query->idWorker);
    //close(query->socket);
    log_debug(informacion->logger, "Comunicacion Cerrada con Query");
    eliminar_paquete(paquete);
    //free(query);
 

}


// ----------------------------------------------------------------------------
void comenzar_a_ejecutar(t_hacerConnect* informacion, int idWorker){
    
    t_query* query = NULL;

    while (query == NULL) {
        log_debug(informacion->logger,"\033[35m EStoy esperando el wait \033[0m\n");
        sem_wait(&sem_queries); // ESPERO A QUE HAYA UNA QUERY
        log_debug(informacion->logger, "\033[35m Pude pasar el wait \033[0m\n");
        
        pthread_mutex_lock(&mutexListaWorkers);
        t_worker* worker= obtener_por_id_worker(lista_workers, informacion->id );
        pthread_mutex_unlock(&mutexListaWorkers);

        if(worker == NULL || worker->desconection){
            log_debug(informacion->logger,   "SE TERMINA HILO WORKER ID: %d", informacion->id);
            sem_post(&sem_queries);
            if(worker) sem_post(&worker->desconexion_worker);
            log_debug(informacion->logger,"hice los signalll");
            //free(informacion);
            return;
        }

        // MANDAR UN SEND DE COMO VOY A MANDARTE UNA QUERY PARA SABER SI ESTA VIVO
        pthread_mutex_lock(&mutexColaQuery);
        if(strcmp(algoritmo_planificacion, "FIFO") == 0){ // PLANIFICO SEGUN ALGORITMO
            if(!list_is_empty(cola_queries)){
                query = list_remove(cola_queries, 0);
            }
        }else{
            query = obtener_menor_prioridad(cola_queries);
        }
        pthread_mutex_unlock(&mutexColaQuery);
    }
    
    /*
// SOLO PARA PRUEBASSS
    char* idsEnCola = string_new();
// string_new() de commons, crea string vacío
    
    for (int i = 0; i < list_size(cola_queries); i++) {
    t_query* q = list_get(cola_queries, i);
    string_append_with_format(&idsEnCola, "%d ", q->id);
    }
// PRUEBAAAAA

    log_info(informacion->logger, "se quito query a la cola, cola actual:  %s", idsEnCola);
*/
    
    query->estado= RUNNING;

    pthread_mutex_lock(&mutexQueryEnWorker); 
    query->idWorker = idWorker;
    list_add(query_en_worker, query);
// AGREGO QUERY A LISTA DE RUNNING
    pthread_mutex_unlock(&mutexQueryEnWorker);

    enviar_query_a_worker(query,informacion, idWorker );
}


// ----------------------------------------------------------------------------
void enviar_query_a_worker(t_query* query,t_hacerConnect* informacion, int idWorker){
    t_buffer* infoQuery = crear_buffer();

    t_paquete* paquete  = crear_paquete(WORKER_ASSIGN_QUERY, infoQuery);

    agregar_a_paquete(paquete,&query->id,sizeof(int));
    agregar_a_paquete(paquete,query->path,strlen(query->path) + 1);
    agregar_a_paquete(paquete,&query->prioridad,sizeof(int));
    agregar_a_paquete(paquete,&query->programCounter,sizeof(int));
    

    int desconexion = enviar_paquete( paquete,  informacion->socket_conexion ,  informacion->logger);
    if (desconexion == -1){
        log_debug(informacion->logger, "WORKER SE DESCONECTO ID: %d", informacion->id);
        //close(informacion->socket_conexion);
        //free(informacion);
    }

    log_info(informacion->logger, "## Se envía la Query %d (%d) al Worker %d", query->id, query->prioridad,idWorker);

    eliminar_paquete(paquete);

    atender_Worker(informacion);

}


// ----------------------------------------------------------------------------
t_query* eliminar_por_id(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_query* query) {
        return query->id == idBuscado;
    }

    return list_remove_by_condition(lista, (void*) coincide_id);
}


// ----------------------------------------------------------------------------
t_worker* eliminar_worker_por_id(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_worker* worker) {
        return worker->id == idBuscado;
    }

    return list_remove_by_condition(lista, (void*) coincide_id);
}


// ----------------------------------------------------------------------------
t_query* obtener_por_id(t_list* lista, int idBuscado) {
    
    bool coincide_id(t_query* query) {
        return query->id == idBuscado;
    }

    return list_find(lista, (void*) coincide_id);
}


// ----------------------------------------------------------------------------
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

