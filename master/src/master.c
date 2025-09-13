#include "master.h"
int cantidadWorkers = 0;
void* atender_conexion(t_hacerConnect* informacion){
    t_list* paqueteHandshake = recibir_paquete(informacion->socket_conexion);
    int* handshake = list_get(paqueteHandshake, 0);
    recv(informacion->socket_conexion, handshake,sizeof(int),MSG_WAITALL);
    switch(*handshake){
        case QC_HANDSHAKE:
        log_info(informacion->logger, "Se ha conectado un Query");
        //atender_Query(informacion);
        break;
        
        case WORKER_HANDSHAKE:
        log_info(informacion->logger, "Se ha conectado un worker");
        //atender_Worker(informacion);
        break;
        
        default:
        log_warning(informacion->logger,"Operacion desconocida");
		break;

    }

    close(informacion->socket_conexion);
    free(informacion);
    return NULL;
}

void atender_Query(t_hacerConnect*  informacion){
   t_list* paqueteQuery = recibir_paquete(informacion->socket_conexion);
   int* codOperacion = list_get(paqueteQuery, 1);
   if (*codOperacion != QUERY_REQUEST){

   }
   

}

void atender_Worker(t_hacerConnect* informacion){
    t_list* paqueteWorker = recibir_paquete(informacion->socket_conexion);
    int* codOperacion =  list_get(paqueteWorker, 0);
    switch (*codOperacion){
        case WORKER_HANDSHAKE:
            cantidadWorkers ++;
            int* idWorker = list_get(paqueteWorker, 1);
            log_info(informacion->logger, "Se ha conectado un worker  ID: %d  CANTIDAD TOTAL DE WORKERS: %d", idWorker, cantidadWorkers);
            
            
    }
}





