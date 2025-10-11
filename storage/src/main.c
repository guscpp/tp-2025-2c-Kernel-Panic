#include "storage.h"

int main(int argc, char* argv[]) {
        if (argc != 2)
        {
            printf("Uso: ./bin/storage [archivo_config]\n");
            return EXIT_FAILURE;
        }

    t_storage* storage = iniciar_storage();

    verificar_storage(storage);

    cola_tareas* cola = crear_cola_tareas(); // crear la cola de tareas

    // cola_tareas* cola_tareas_global;
    // buscar todas las funciones que utilizaban la variable global cola_tareas_global y modificar esas funciones para que en la lista de parametros se le pueda pasar lo que esta en cola_tareas* cola = crear_cola_tareas();
    // esto es muy parecido como resolvimos el storage global

    args_hilo_consumidor* args_consumidor = malloc(sizeof(args_hilo_consumidor));
    args_consumidor->storage = storage;
    args_consumidor->cola_tareas = cola;

    pthread_t hilo_consumidor; // crea el unico hilo consumidor
    if(pthread_create(&hilo_consumidor, NULL, rutina_operaciones, args_consumidor) != 0){
        log_error(storage->logger, "Error al crear el hilo consumidor");
        return EXIT_FAILURE;
    }

    pthread_detach(hilo_consumidor);
    log_info(storage->logger, "Hilo consumidor creado"); 
    
    int storage_fd = iniciar_servidor(storage->puerto_escucha);  //socket, bind, listen    inicia el servidor 
    log_info(storage->logger, "Servidor listo");

    while(1){
        int worker_fd = esperar_cliente(storage_fd);//acept
        if(worker_fd <0 ){
            log_error(storage->logger, "Error al esperar cliente");
            continue;
        }
        log_info(storage->logger, "Cliente conectado");

        pthread_t hilo_productor;
        args_hilo_productor* args_productor = malloc(sizeof(args_hilo_productor));
        args_productor->socket_cliente = worker_fd;
        args_productor->storage = storage;
        args_productor->cola = cola;

        if(pthread_create(&hilo_productor, NULL, rutina_recepcion, args_productor) != 0){
            log_error(storage->logger, "Error al crear el hilo productor");
            close(worker_fd);
            free(args_productor);
        }else{
            pthread_detach(hilo_productor);
            log_info(storage->logger, "Hilo productor creado");
        }
    }
    log_info(storage->logger, "cerrando servidor..."); // esto llega cuando se terminada las operaciones 
    liberar_storage(storage);
    destruir_cola_tareas(cola);

    return 0;


      

    //log_info(storage->logger, "antes de enviar tamanio");
    //enviar_tamanio_paquete_aworker(worker_fd, storage);   hay que ver que onda con worker_fd si dejarlo como global o adentro del while 1 
}
