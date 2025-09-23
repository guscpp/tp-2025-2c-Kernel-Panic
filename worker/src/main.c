#include "../include/worker.h"

int main(int argc, char* argv[]) {
    if (argc != 3)
    {
        printf("Uso: ./bin/worker [archivo_config] [ID Worker]\n");
        return EXIT_FAILURE;
    }


    int id_worker = atoi(argv[2]);
    t_worker* w = inicializar_worker(id_worker);
    t_memoria_interna* m = crear_memoria(w->logger);

    pthread_t ciclo_instrucciones;

    log_info(w->logger, "Verificar funcionamiento logger");

    //Solo logs de prueba: 
    verificar_worker(w);
 
    //Master: crear la conexion
    w->master_socket = crear_conexion(w->logger, w->ip_master, w->puerto_master); 
    t_buffer* buffer1 = crear_buffer();
    t_paquete* packetHandshake = crear_paquete(WORKER_HANDSHAKE, buffer1);
    enviar_paquete(packetHandshake, w->master_socket, w->logger);
    eliminar_paquete(packetHandshake);

    //Master: enviarle ID de este worker, recibir path a query
    buffer1 = crear_buffer();
    t_paquete* packetID = crear_paquete(WORKER_ID, buffer1);
    agregar_a_paquete(packetID, &w->id_worker, sizeof(int));
    enviar_paquete(packetID, w->master_socket, w->logger);
    eliminar_paquete(packetID);


    //Storage: crear la conexion
    w->storage_socket = crear_conexion(w->logger, w->ip_storage, w->puerto_storage); //socket y connect
    t_buffer* buffer2 = crear_buffer();
    t_paquete* packetHandshake2 = crear_paquete(WORKER_HANDSHAKE, buffer2);
    enviar_paquete(packetHandshake2, w->storage_socket, w->logger);
    eliminar_paquete(packetHandshake2);

    rtas_storage(w->storage_socket, w);


    //MAster: recibir path de master
        //recibir_path_de_query(w->master_socket, w->logger); Este es el de la funcion anterior de recibir_path_de_query
    
    t_ejecucion* datos_ejecucion = malloc(sizeof(t_ejecucion));
    datos_ejecucion->w = w;
    datos_ejecucion->master_socket = w->master_socket;
    int err= pthread_create(&ciclo_instrucciones, 
                            NULL, 
                            ejecutar_query, 
                            datos_ejecucion);
    if(err != 0){
        log_info(w->logger, "No se creo el hilo del ciclo de instrucciones");
    }
    pthread_detach(ciclo_instrucciones);

   
    return 0;
}