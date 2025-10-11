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

void* rutina_recepcion(void* args){ // se encarga de recibir los parametros necesarios que necesita cada operacion
    args_hilo_productor* argumentos = (args_hilo_productor*) args;
    int socket_worker = argumentos->socket_cliente;
    t_storage* storage_global = argumentos->storage;
    cola_tareas* cola_global = argumentos->cola;

    free(args);

    log_info(storage_global->logger, "PRODUCTOR: Nuevo hilo para atender woker en [Socket %d]", socket_worker);

    while(1){
        op_code codigo_operacion = recibir_operacion(socket_worker);
        if(codigo_operacion <= 0){
            if(codigo_operacion == 0){
                log_info(storage_global->logger, "El worker en [Socket %d] se desconecto", socket_worker);
            }else{
                log_error(storage_global->logger, "Error al recibir operacion del worker en [Socket %d]", socket_worker);
            }
            break;
        }
        // falta implementar la recepcion de parametros con switch case segun codigo_operacion.  (medio raro hay que charlar para ver como hacerlo)
        void* parametros = NULL;
        switch (codigo_operacion)
        {
        case STORAGE_CREATE_FILE:
            parametros = recibir_parametros_creacion_de_file(socket_worker);
            break;
        case STORAGE_TRUNCATE:
            parametros = recibir_parametros_truncado_de_archivo(socket_worker);
            break;
        case STORAGE_TAG:
            parametros = recibir_parametros_tag_de_file(socket_worker);
            break;
        case STORAGE_COMMIT:
            parametros = recibir_parametros_commit_de_un_tag(socket_worker);
            break;
        // case ESTRUCTURA_DE_BLOQUE:
        //     parametros = recibir_parametros_estructura_de_bloque(socket_worker);
        //     break;
        // case LECTURA_DE_BLOQUE:
        //     parametros = recibir_parametros_lectura_de_bloque(socket_worker);
        //     break;
        case STORAGE_DELETE:
            parametros = recibir_parametros_eliminar_un_tag(socket_worker);
            break;
        
        default:
            log_error(storage_global->logger, "Operacion desconocida %d recibida del worker en [Socket %d]", codigo_operacion, socket_worker);
            close(socket_worker);
            return NULL;
        }


        Tarea* nueva_tarea = malloc(sizeof(Tarea));
        nueva_tarea->codigo_operacion = codigo_operacion;
        nueva_tarea->socket_cliente = socket_worker;
        nueva_tarea->parametros = parametros; // hay que implmentar una funcion que sea recibir parametros

        encolar_tarea(cola_global, nueva_tarea);

        log_info(storage_global->logger, "PRODUCTOR: Tarea encolada para worker en [Socket %d] con operacion %d", socket_worker, codigo_operacion);
    }
    close(socket_worker);
    log_info(storage_global->logger, "Hilo para atender worker en [Socket %d] finalizado", socket_worker);
    return NULL;
}

void* rutina_operaciones(void* args){ // se encarga de ejecutar las operaciones que recibe el hilo productor
    args_hilo_consumidor* argumentos = (args_hilo_consumidor*) args;
    t_storage* storage_global = argumentos->storage;
    cola_tareas* cola_global = argumentos->cola_tareas;
    free(args);
    
    log_info(storage_global->logger, "CONSUMIDOR: Hilo consumidor iniciado");
    while(1){
        Tarea* tarea = desencolar_tarea(cola_global);
        if(tarea == NULL){
            log_error(storage_global->logger, "Error al desencolar tarea");
            continue;
        }
        log_info(storage_global->logger, "CONSUMIDOR: Procesando tarea de worker en [Socket %d] con operacion %d", tarea->socket_cliente, tarea->codigo_operacion);
        // falta implementar la logica de cada operacion con switch case segun codigo_operacion.  (medio raro hay que charlar para ver como hacerlo)
        switch (tarea->codigo_operacion)
        {
        case STORAGE_CREATE_FILE:
            // implementar logica de creacion de file
            log_info(storage_global->logger, "Operacion STORAGE_CREATE_FILE");
            break;
        case STORAGE_TRUNCATE:
            // implementar logica de truncado de archivo
            log_info(storage_global->logger, "Operacion STORAGE_TRUNCATE");
            break;
        case STORAGE_TAG:
            // implementar logica de tag de file
            log_info(storage_global->logger, "Operacion STORAGE_TAG");
            break;
        case STORAGE_COMMIT:
            // implementar logica de commit de un tag
            log_info(storage_global->logger, "Operacion STORAGE_COMMIT");
            break;
        // case ESTRUCTURA_DE_BLOQUE:
        //     // implementar logica de estructura de bloque
        // log_info(storage_global->logger, "Operacion ESTRUCTURA_DE_BLOQUE");
        // case LECTURA_DE_BLOQUE:
        //     // implementar logica de lectura de bloque
        // log_info(storage_global->logger, "Operacion STORAGE_DELETE");
        //     break;
        case STORAGE_DELETE:
            // implementar logica de eliminar un tag
            log_info(storage_global->logger, "Operacion STORAGE_DELETE");
            break;
        
        default:
            log_error(storage_global->logger, "Operacion desconocida %d recibida del worker en [Socket %d]", tarea->codigo_operacion, tarea->socket_cliente);
            close(tarea->socket_cliente);
            free(tarea);
            continue;
        }

        // liberar recursos de la tarea
        free(tarea->parametros); // liberar los parametros si es necesario
        free(tarea);

        log_info(storage_global->logger, "CONSUMIDOR: Tarea procesada para worker en [Socket %d] con operacion %d", tarea->socket_cliente, tarea->codigo_operacion);
    }



}













 