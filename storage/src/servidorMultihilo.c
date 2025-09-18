#include "servidorMultihilo.h"

pthread_mutex_t socket_worker_temp;
int socket_client_temp = 1;

void rutina_recepcion(int socket_escucha){
    pthread_t hilo_ejecucion;
    int error;
    int aux_socket_worker;
    pthread_mutex_init(&socket_worker_temp, NULL);

    while(1){
        aux_socket_worker = esperar_cliente(socket_escucha);
        pthread_mutex_lock(&socket_worker_temp);
        socket_client_temp = aux_socket_worker;

        error = pthread_create(&hilo_ejecucion, NULL, rutina_recepcion, NULL); // rutina recepcion a revisar
        if (error != 0){
            log_error(storage_general->logger, "error al crear el hilo_ejecucion");
        }else{
            pthread_detach(hilo_ejecucion);
        }
        pthread_mutex_destroy(&socket_worker_temp);
    }
}

 