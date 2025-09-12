#ifndef MASTER_H_
#define MASTER_H_

#include "../../utils/include/utils.h"

typedef struct{
    t_log* logger;
    int socket_conexion;
} hacerConect;

void* atender_conexion(hacerConect*);

// int cantidadWorkers = 0;
// int id_Query = 0;



#endif /* CLIENTE_H_ */