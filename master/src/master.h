#ifndef MASTER_H_
#define MASTER_H_

#include "../../utils/include/utils.h"
#include <unistd.h>
#include <semaphore.h>

typedef struct{
    t_log* logger;
    int socket_conexion;
} t_hacerConnect;

typedef struct {
    int id;               // ID único asignado por el Master
    char* path;           // Ruta del archivo de la query
    int prioridad;        // Prioridad menor número = mayor prioridad
    int socket; 
    int programCounter ;
    int idWorker;
    } t_query;

void* atender_conexion(void*);
void atender_Query(t_hacerConnect* );
void atender_Worker(t_hacerConnect* );
void inicializar_semaforos( t_log*);
void* atender_desconexion_query(void*);
void asignar_id_query(int* );
void comenzar_a_ejecutar(t_hacerConnect* , int);
void enviar_query_a_worker(t_query*,t_hacerConnect*, int);
t_query* eliminar_por_id(t_list* , int );
void query_completado_con_exito(t_query* , t_hacerConnect* );
#endif /* CLIENTE_H_ */