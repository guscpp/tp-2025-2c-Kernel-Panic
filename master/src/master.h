#ifndef MASTER_H_
#define MASTER_H_

#include "../../utils/include/utils.h"
#include <unistd.h>

typedef struct{
    t_log* logger;
    int socket_conexion;
} t_hacerConnect;

void* atender_conexion(t_hacerConnect*);
void atender_Query(t_hacerConnect* );
void atender_Worker(t_hacerConnect* );
// int cantidadWorkers = 0;
// int id_Query = 0;



#endif /* CLIENTE_H_ */