#include "servidorMultihilo.h"


void rutina_recepcion(t_storage* storage, int storage_fd){ // se encarga de aceptar conexiones y crear hilos para cada worker que se conecte
    pthread_t hilo_ejecucion;
    int aux_socket_worker_temp;

    log_info(storage->logger, "Hilo recepcion listo");

    while(1){
        aux_socket_worker_temp = esperar_cliente(storage_fd);//acept
        if(aux_socket_worker_temp < 0 ){
            log_error(storage->logger, "Error al esperar cliente");
            continue;
        }
        log_info(storage->logger, "Cliente conectado");

        args_hilo_worker* args = malloc(sizeof(args_hilo_worker));
        args->socket_cliente = aux_socket_worker_temp;
        args->storage = storage;

        if(pthread_create(&hilo_ejecucion, NULL, rutina_operaciones, args) != 0){
            log_error(storage->logger, "Error al crear el hilo ejecucion");
        }else{
            pthread_detach(hilo_ejecucion);
            log_info(storage->logger, "Hilo ejecucion creado");
        }
    }
}

void* rutina_operaciones(void* args){ // se encarga de recibir las operaciones de los workers y ejecutar la logica correspondiente
    args_hilo_worker* argumentos = (args_hilo_worker*) args;
    int socket_cliente = argumentos->socket_cliente;
    t_storage* storage = argumentos->storage;
    free(argumentos);
    
    t_list* paquete = recibir_paquete(socket_cliente);
    int codigo_operacion = *(int*) list_get(paquete, 0);
    if (codigo_operacion == WORKER_HANDSHAKE)
        log_info(storage->logger, "Handshake recibido, opcode: %d", codigo_operacion);
    else if (paquete == NULL) {
        log_error(storage->logger, "Error al recibir paquete del worker en [Socket %d]", socket_cliente);
        close(socket_cliente);
        return NULL;
    }
    // list_destroy_and_destroy_elements(paquete, free);
  
    log_info(storage->logger, "Hilo ejecucion listo para recibir operaciones de worker en [Socket %d]", socket_cliente);
    

    while(1)
    {
        printf("Inicio del while(1)\n"); //debug        
        t_list* paquete = recibir_paquete(socket_cliente); // segundo recibe que hace es recibir la operacion
        if(!paquete) break;
        int codigo_operacion = *(int*) list_get(paquete, 0);
        //list_destroy_and_destroy_elements(paquete, free);
        printf("opcode: %i\n", codigo_operacion);

        switch (codigo_operacion)
        {
        case STORAGE_GET_BLOCK_SIZE:
            
            log_info(storage->logger, "Operacion STORAGE_GET_BLOCK_SIZE");
            enviar_tamanio_paquete_aworker(storage, socket_cliente);
            break;
            
        case STORAGE_CREATE_FILE:
            if(crear_file(storage, paquete)){
                log_info(storage->logger, "File creado exitosamente");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_OK, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);

            }else{
                log_error(storage->logger, "Error al crear el file");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_ERROR, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }
            
            break;

        case STORAGE_TRUNCATE:
            // implementar logica de truncado de archivo
            
            log_info(storage->logger, "Operacion STORAGE_TRUNCATE");
            break;

        case STORAGE_TAG:
            // implementar logica de tag de file
            
            log_info(storage->logger, "Operacion STORAGE_TAG");
            break;

        case STORAGE_COMMIT:
            // implementar logica de commit de un tag

            log_info(storage->logger, "Operacion STORAGE_COMMIT");
            break;

        case STORAGE_READ_BLOCK:
            // implementar logica de estructura de bloque
            
            log_info(storage->logger, "Operacion STORAGE_READ_BLOCK");
            break;

        case STORAGE_WRITE_BLOCK:
            // implementar logica de lectura de bloque
            
            log_info(storage->logger, "Operacion STORAGE_WRITE_BLOCK");
            break;

        case STORAGE_DELETE:
            // implementar logica de eliminar un tag
            
            log_info(storage->logger, "Operacion STORAGE_DELETE");
            break;
        
        default:
            log_error(storage->logger, "Operacion desconocida %d recibida del worker en [Socket %d]", codigo_operacion, socket_cliente);

        }

        list_destroy_and_destroy_elements(paquete, free);
    }
    
    close(socket_cliente);
    log_info(storage->logger, "Hilo worker finalizado [Socket %d]", socket_cliente);
    return NULL;


}












 