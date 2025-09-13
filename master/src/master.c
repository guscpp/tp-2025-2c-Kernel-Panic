#include "master.h"

void* atender_conexion(t_hacerConnect* informacion){
    int handshake ;
    recv(informacion->socket_conexion, &handshake,sizeof(int),MSG_WAITALL);
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

//void atender_Query(hacerConect* informacion){

//}





