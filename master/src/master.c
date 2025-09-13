#include "master.h"

void* atender_conexion(t_hacerConnect* informacion){
    t_list* paqueteHandshake = recibir_paquete(informacion->socket_conexion);
    int* handshake = list_get(paqueteHandshake, 1);
    recv(informacion->socket_conexion, handshake,sizeof(int),MSG_WAITALL);
    switch(handshake){
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
    t_paquete* paquete = malloc(sizeof(t_paquete));
    recv(informacion->socket_conexion, &paquete ,sizeof(t_paquete),MSG_WAITALL);
    

}

void atender_Worker(t_hacerConnect*){

}





