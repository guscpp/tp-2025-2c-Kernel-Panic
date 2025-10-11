#include "servidorMultihilo.h"
 //solo lo comente para poder levantar worker
pthread_mutex_t mutex_cola;
pthread_cond_t hay_tarea_cond;

void rutina_recepcion(void* args){ // se encarga de recibir los parametros necesarios que necesita cada operacion
    int socket_worker = *(int*)args;
    free(args);

    log_info(storage_general->logger, "PRODUCTOR: Nuevo hilo para atender woker en [Socket %d]", socket_worker);

    void* parametros = NULL; // hay que implmentar una funcion que sea recibir parametros

    while(1){
        op_code codigo_operacion = recibir_operacion(socket_worker);
        if(codigo_operacion <= 0){
            if(codigo_operacion == 0){
                log_info(storage_general->logger, "El worker en [Socket %d] se desconecto", socket_worker);
            }else{
                log_error(storage_general->logger, "Error al recibir operacion del worker en [Socket %d]", socket_worker);
            }
            break;
        }
        // falta implementar la recepcion de parametros con switch case segun codigo_operacion.  (medio raro hay que charlar para ver como hacerlo)
        Tarea* nueva_tarea = malloc(sizeof(Tarea));
        nueva_tarea->codigo_operacion = codigo_operacion;
        nueva_tarea->socket_cliente = socket_worker;
        nueva_tarea->parametros = parametros; // hay que implmentar una funcion que sea recibir parametros

        encolar_tarea(cola_tareas_global, nueva_tarea);

        log_info(storage_general->logger, "PRODUCTOR: Tarea encolada para worker en [Socket %d] con operacion %d", socket_worker, codigo_operacion);
    }
    close(socket_worker);
    log_info(storage_general->logger, "Hilo para atender worker en [Socket %d] finalizado", socket_worker);
    return NULL;
}

void rutina_operaciones(){ // se encarga de ejecutar las operaciones que recibe el hilo productor
    return NULL;
}










 