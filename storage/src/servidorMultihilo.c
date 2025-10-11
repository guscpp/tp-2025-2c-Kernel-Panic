#include "servidorMultihilo.h"
 //solo lo comente para poder levantar worker
pthread_mutex_t mutex_cola;
pthread_cond_t hay_tarea_cond;

void* recibir_parametros_creacion_de_file(int socket_cliente){
    // falta implementar la logica
    return NULL;
}

void* recibir_parametros_truncado_de_archivo(int socket_cliente){
    // falta implementar la logica
    return NULL;
}

void* recibir_parametros_tag_de_file(int socket_cliente){
    // falta implementar la logica
    return NULL;
}

void* recibir_parametros_commit_de_un_tag(int socket_cliente){
    // falta implementar la logica
    return NULL;
}

void* recibir_parametros_estructura_de_bloque(int socket_cliente){
    // falta implementar la logica
    return NULL;
}   

void* recibir_parametros_lectura_de_bloque(int socket_cliente){
    // falta implementar la logica
    return NULL;
}
void* recibir_parametros_eliminar_un_tag(int socket_cliente){
    // falta implementar la logica
    return NULL;
}

void rutina_recepcion(void* args){ // se encarga de recibir los parametros necesarios que necesita cada operacion
    int socket_worker = *(int*)args;
    free(args);

    log_info(storage_general->logger, "PRODUCTOR: Nuevo hilo para atender woker en [Socket %d]", socket_worker);

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
        void* parametros = NULL;
        switch (codigo_operacion)
        {
        case CREACION_DE_FILE:
            parametros = recibir_parametros_creacion_de_file(socket_worker);
            break;
        case TRUNCADO_DE_ARCHIVO:
            parametros = recibir_parametros_truncado_de_archivo(socket_worker);
            break;
        case TAG_DE_FILE:
            parametros = recibir_parametros_tag_de_file(socket_worker);
            break;
        case COMMIT_DE_UN_TAG:
            parametros = recibir_parametros_commit_de_un_tag(socket_worker);
            break;
        case ESTRUCTURA_DE_BLOQUE:
            parametros = recibir_parametros_estructura_de_bloque(socket_worker);
            break;
        case LECTURA_DE_BLOQUE:
            parametros = recibir_parametros_lectura_de_bloque(socket_worker);
            break;
        case ELIMINAR_UN_TAG:
            parametros = recibir_parametros_eliminar_un_tag(socket_worker);
            break;
        
        default:
            log_error(storage_general->logger, "Operacion desconocida %d recibida del worker en [Socket %d]", codigo_operacion, socket_worker);
            close(socket_worker);
            return;
        }


        Tarea* nueva_tarea = malloc(sizeof(Tarea));
        nueva_tarea->codigo_operacion = codigo_operacion;
        nueva_tarea->socket_cliente = socket_worker;
        nueva_tarea->parametros = parametros; // hay que implmentar una funcion que sea recibir parametros

        encolar_tarea(cola_tareas_global, nueva_tarea);

        log_info(storage_general->logger, "PRODUCTOR: Tarea encolada para worker en [Socket %d] con operacion %d", socket_worker, codigo_operacion);
    }
    close(socket_worker);
    log_info(storage_general->logger, "Hilo para atender worker en [Socket %d] finalizado", socket_worker);
    return;
}

void rutina_operaciones(){ // se encarga de ejecutar las operaciones que recibe el hilo productor
    log_info(storage_general->logger, "CONSUMIDOR: Hilo consumidor iniciado");
    while(1){
        Tarea* tarea = desencolar_tarea(cola_tareas_global);
        if(tarea == NULL){
            log_error(storage_general->logger, "Error al desencolar tarea");
            continue;
        }
        log_info(storage_general->logger, "CONSUMIDOR: Procesando tarea de worker en [Socket %d] con operacion %d", tarea->socket_cliente, tarea->codigo_operacion);
        // falta implementar la logica de cada operacion con switch case segun codigo_operacion.  (medio raro hay que charlar para ver como hacerlo)
        switch (tarea->codigo_operacion)
        {
        case CREACION_DE_FILE:
            // implementar logica de creacion de file
            break;
        case TRUNCADO_DE_ARCHIVO:
            // implementar logica de truncado de archivo
            break;
        case TAG_DE_FILE:
            // implementar logica de tag de file
            break;
        case COMMIT_DE_UN_TAG:
            // implementar logica de commit de un tag
            break;
        case ESTRUCTURA_DE_BLOQUE:
            // implementar logica de estructura de bloque
            break;
        case LECTURA_DE_BLOQUE:
            // implementar logica de lectura de bloque
            break;
        case ELIMINAR_UN_TAG:
            // implementar logica de eliminar un tag
            break;
        
        default:
            log_error(storage_general->logger, "Operacion desconocida %d recibida del worker en [Socket %d]", tarea->codigo_operacion, tarea->socket_cliente);
            close(tarea->socket_cliente);
            free(tarea);
            continue;
        }

        // liberar recursos de la tarea
        free(tarea->parametros); // liberar los parametros si es necesario
        free(tarea);

        log_info(storage_general->logger, "CONSUMIDOR: Tarea procesada para worker en [Socket %d] con operacion %d", tarea->socket_cliente, tarea->codigo_operacion);
    }



}













 