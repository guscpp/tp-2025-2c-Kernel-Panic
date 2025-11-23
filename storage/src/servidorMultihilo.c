#include "servidorMultihilo.h"


void rutina_recepcion(t_storage* storage, int storage_fd){ // se encarga de aceptar conexiones y crear hilos para cada worker que se conecte
    pthread_t hilo_ejecucion;
    int aux_socket_worker_temp;

    log_debug(storage->logger, "Hilo recepcion listo");

    while(1){
        aux_socket_worker_temp = esperar_cliente(storage_fd);//acept
        if(aux_socket_worker_temp < 0 ){
            log_error(storage->logger, "Error al esperar cliente");
            continue;
        }

        //SO_KEEPALIVE el SO cierra el sockets cuando se cae el Worker, chau hilos zombie
        int keepalive = 1; // 1 para habilitar, 0 para deshabilitar
        if (setsockopt(aux_socket_worker_temp, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
            log_error(storage->logger, "setsockopt keepalive falló para socket %d", aux_socket_worker_temp);
        } else {
             log_debug(storage->logger, "Keepalive habilitado para socket %d", aux_socket_worker_temp);
        }
        t_list* paquete_para_id = recibir_paquete(aux_socket_worker_temp);
        if(!paquete_para_id){
            log_error(storage->logger, "Error al recibir paquete de ID del worker en [Socket %d]", aux_socket_worker_temp);
            close(aux_socket_worker_temp);
            continue;
        }
        int codigo_operacion = *(int*) list_get(paquete_para_id, 0);
        if(codigo_operacion != WORKER_ID){
            log_error(storage->logger, "Operacion invalida al recibir ID del worker en [Socket %d]", aux_socket_worker_temp);
            list_destroy_and_destroy_elements(paquete_para_id, free);
            close(aux_socket_worker_temp);
            continue;
        }
        int worker_id = *(int*) list_get(paquete_para_id, 1);
        
        pthread_mutex_lock(&storage->mutex_workers);
        storage->cantidad_workers++;
        pthread_mutex_unlock(&storage->mutex_workers);

        log_info(storage->logger, "##Se conecta el Worker <%d> - Cantidad de Workers: <%d>", worker_id, storage->cantidad_workers);
        log_debug(storage->logger, "Cliente conectado");

        t_worker_context* contexto = malloc(sizeof(t_worker_context));
        contexto->socket_cliente = aux_socket_worker_temp;
        contexto->storage = storage;
        contexto->worker_id = worker_id;

        if(pthread_create(&hilo_ejecucion, NULL, rutina_operaciones, contexto) != 0){
            log_error(storage->logger, "Error al crear el hilo ejecucion");
            free(args);
            close(aux_socket_worker_temp);
        }else{
            pthread_detach(hilo_ejecucion);
            log_debug(storage->logger, "Hilo ejecucion creado");
        }
    }
}

void* rutina_operaciones(void* args){ // se encarga de recibir las operaciones de los workers y ejecutar la logica correspondiente
    t_worker_context* contexto = (t_worker_context*) args;
    int socket_cliente = contexto->socket_cliente;
    t_storage* storage = contexto->storage;
    int worker_id = contexto->worker_id;
    free(args);
    
    t_list* paquete = recibir_paquete(socket_cliente);
    int codigo_operacion = *(int*) list_get(paquete, 0);
    if (codigo_operacion == WORKER_HANDSHAKE) {
        log_debug(storage->logger, "Handshake recibido, opcode: %d", codigo_operacion);
        list_destroy_and_destroy_elements(paquete, free);
    } else if (paquete == NULL) {
        log_error(storage->logger, "Error al recibir paquete del worker en [Socket %d]", socket_cliente);
        list_destroy_and_destroy_elements(paquete, free);
        close(socket_cliente);
        return NULL;
    }

    log_debug(storage->logger, "Hilo ejecucion listo para recibir operaciones de worker en [Socket %d]", socket_cliente);


    while(1)
    {
        //printf("Inicio del while(1)\n"); //debug        
        t_list* paquete = recibir_paquete(socket_cliente); // segundo recibe que hace es recibir la operacion
        if(!paquete) break;
        int codigo_operacion = *(int*) list_get(paquete, 0);
        //list_destroy_and_destroy_elements(paquete, free);
        printf("opcode: %i\n", codigo_operacion);

        switch (codigo_operacion)
        {
        case STORAGE_GET_BLOCK_SIZE:
            log_debug(storage->logger, "Operacion STORAGE_GET_BLOCK_SIZE");
            enviar_tamanio_paquete_aworker(storage, socket_cliente);
            break;
            
        case STORAGE_CREATE_FILE:
            if(crear_file(storage, paquete)){
                log_debug(storage->logger, "File creado exitosamente");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_OK_CREATE_FILE, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);

            }else{
                log_error(storage->logger, "Error al crear el file");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_ERROR_CREATE_FILE, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }
            break;

        case STORAGE_TRUNCATE:
            if(truncar_file(storage, paquete)){
                log_debug(storage->logger, "File truncado exitosamente");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_OK_TRUNCATE, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }else{
                log_error(storage->logger, "Error al truncar el file");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_ERROR_TRUNCATE, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }
            break;
            
        case STORAGE_TAG:
            if(tag_file(storage, paquete)){
                log_debug(storage->logger, "Tag creado exitosamente");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_OK_TAG, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }else{
                log_error(storage->logger, "Error al crear el tag");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_ERROR_TAG, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }
            break;

        case STORAGE_COMMIT:
            if (realizar_commit(storage, paquete)) {
                log_debug(storage->logger, "Commit realizado exitosamente");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_OK_COMMIT, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            } else {
                log_error(storage->logger, "Error al realizar commit");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_ERROR_COMMIT, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }
            break;

        case STORAGE_FLUSH:
            if(flush_archivo(storage, paquete)){
                log_debug(storage->logger, "FLUSH realizado exitosamente");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_OK_FLUSH, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            } else {
                log_error(storage->logger, "Error al realizar FLUSH");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_ERROR_FLUSH, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }
            break;

        case STORAGE_READ_BLOCK:
            void* contenido_bloque = NULL;
            int tamanio_bloque = storage->tamanio_bloque;
            if (leer_bloque(storage, paquete, &contenido_bloque, &tamanio_bloque)) {
                log_debug(storage->logger, "Contenido leido: %p, tamaño: %d", contenido_bloque, tamanio_bloque);
                t_buffer* buffer_resp = crear_buffer();
                t_paquete* paquete_resp = crear_paquete(STORAGE_SEND_OK_READ_BLOCK, buffer_resp);
                agregar_a_paquete(paquete_resp, contenido_bloque, tamanio_bloque);
                log_debug(storage->logger, "Contenido leido: %p, tamaño: %d", contenido_bloque, tamanio_bloque);
                enviar_paquete(paquete_resp, socket_cliente, storage->logger);
                eliminar_paquete(paquete_resp);
                free(contenido_bloque);
            } else {
                t_buffer* buf_err = crear_buffer();
                t_paquete* pkt_err = crear_paquete(STORAGE_SEND_ERROR_READ_BLOCK, buf_err);
                enviar_paquete(pkt_err, socket_cliente, storage->logger);
                eliminar_paquete(pkt_err);
            }
            break;

        case STORAGE_WRITE_BLOCK:
            if(escribir_bloque(storage, paquete)){
                log_debug(storage->logger, "Bloque escrito exitosamente");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_OK_WRITE_BLOCK, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }else{
                log_error(storage->logger, "Error al escribir el bloque");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_ERROR_WRITE_BLOCK, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }
            break;

        case STORAGE_DELETE:
            if(eliminar_file_tag(storage, paquete)){
                log_debug(storage->logger, "File:Tag eliminado exitosamente");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_OK_DELETE, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            }else{
                log_error(storage->logger, "Error al eliminar el File:Tag");
                t_buffer* respuesta_buffer = crear_buffer();
                t_paquete* paquete_respuesta = crear_paquete(STORAGE_SEND_ERROR_DELETE, respuesta_buffer);
                enviar_paquete(paquete_respuesta, socket_cliente, storage->logger);
                eliminar_paquete(paquete_respuesta);
            } 
            break;
        
        default:
            log_error(storage->logger, "Operacion desconocida %d recibida del worker en [Socket %d]", codigo_operacion, socket_cliente);
        }

        list_destroy_and_destroy_elements(paquete, free);
    }

    close(socket_cliente);
    log_debug(storage->logger, "Hilo worker finalizado [Socket %d]", socket_cliente);
    
    pthread_mutex_lock(&storage->mutex_workers);
    storage->cantidad_workers++;
    pthread_mutex_unlock(&storage->mutex_workers);

    log_info(storage->logger, "##Se desconecta el Worker <%d> - Cantidad de Workers: <%d>", worker_id, storage->cantidad_workers);
    return NULL;
}