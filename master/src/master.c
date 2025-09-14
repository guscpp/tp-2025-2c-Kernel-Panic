#include "master.h"
int cantidadWorkers = 0;
void* atender_conexion(t_hacerConnect* informacion){
    t_list* paqueteHandshake = recibir_paquete(informacion->socket_conexion);
    int* handshake = list_get(paqueteHandshake, 0);
    switch(*handshake){
        case QC_HANDSHAKE:
        log_info(informacion->logger, "Se ha conectado un Query");
        atender_Query(informacion);
        break;
        
        case WORKER_HANDSHAKE:
        log_info(informacion->logger, "Se ha conectado un worker");
        atender_Worker(informacion);
        break;
        
        default:
        log_warning(informacion->logger,"Operacion desconocida");
		break;

    }

    close(informacion->socket_conexion);
    free(informacion);
    return NULL;
}
int query_id = 1;
char* path_query = "path :)";
int prioridad = 2;

void atender_Query(t_hacerConnect*  informacion){
   t_list* paqueteQuery = recibir_paquete(informacion->socket_conexion);
   int* codOperacion = list_get(paqueteQuery, 0);
   if (*codOperacion != QUERY_REQUEST){
        log_warning(informacion->logger,"Operacion desconocida");
   }
   char* path = list_get(paqueteQuery, 1);
   int* prioridad = list_get(paqueteQuery, 2);
   log_info(informacion->logger, "Se conecta un Query Control para ejecutar la Query %s con prioridad %d - Id asignado: %d ", path, *prioridad, query_id);
   list_destroy_and_destroy_elements(paqueteQuery, free);
}


void atender_Worker(t_hacerConnect* informacion){
    t_list* paqueteWorker = recibir_paquete(informacion->socket_conexion);
    int* codOperacion =  list_get(paqueteWorker, 0);
    switch (*codOperacion){
        case WORKER_HANDSHAKE:{
            cantidadWorkers ++;
            int* idWorker = list_get(paqueteWorker, 1);
            log_info(informacion->logger, "Se ha conectado un worker  ID: %d  CANTIDAD TOTAL DE WORKERS: %d", *idWorker , cantidadWorkers);
            t_buffer* infoPath = crear_buffer();
            t_paquete* paquete  = crear_paquete(WORKER_ASSIGN_QUERY, infoPath);
            agregar_a_paquete(paquete,&query_id,sizeof(int));
            agregar_a_paquete(paquete,path_query,strlen(path_query) + 1);
            agregar_a_paquete(paquete,&prioridad,sizeof(int));
            enviar_paquete( paquete,  informacion->socket_conexion ,  informacion->logger);
            log_info(informacion->logger, "se ha enviado al workerrrrr");
            eliminar_paquete(paquete);
            list_destroy_and_destroy_elements(paqueteWorker, free);
            break;
        }
    }
}





