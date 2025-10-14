#include "../include/worker.h"

int main(int argc, char* argv[]) {
    
    if (argc != 3)
    {
        printf("Uso: ./bin/worker [archivo_config] [ID Worker]\n");
        return EXIT_FAILURE;
    }

    int id_worker = atoi(argv[2]);
    t_worker* w = inicializar_worker(id_worker);
    

    int block_size = 128; // <-- valor hardcodeado para test
    t_memoria_interna* m = crear_memoria(
        w->logger,
        config_get_int_value(w->config, "TAM_MEMORIA"),
        config_get_int_value(w->config, "RETARDO_MEMORIA"),
        config_get_string_value(w->config, "ALGORITMO_REEMPLAZO"),
        block_size
    );
    w->mem = m;

    t_query_interpreter*  query_interpreter =  query_interpreter_crear(w->logger); //tiene pc y un verificador de interrupciones
    w->interpreter = query_interpreter; 

    pthread_t ciclo_instrucciones; 
    pthread_t hilo_interrupciones;
    log_info(w->logger, "Verificar funcionamiento logger");

    //Solo logs de prueba: 
    verificar_worker(w);
 
    //Master: crear la conexion
    w->master_socket = crear_conexion(w->logger, w->ip_master, string_itoa(w->puerto_master)); 
    t_buffer* buffer1 = crear_buffer();
    t_paquete* packetHandshake = crear_paquete(WORKER_HANDSHAKE, buffer1);
    enviar_paquete(packetHandshake, w->master_socket, w->logger);
    eliminar_paquete(packetHandshake);

    //Master: enviarle ID de este worker
    buffer1 = crear_buffer();
    t_paquete* packetID = crear_paquete(WORKER_ID, buffer1);
    agregar_a_paquete(packetID, &w->id_worker, sizeof(int));
    enviar_paquete(packetID, w->master_socket, w->logger);
    eliminar_paquete(packetID);


    //Storge: crear la conexion
    w->storage_socket = crear_conexion(w->logger, w->ip_storage, w->puerto_storage); //socket y connect
    t_buffer* buffer2 = crear_buffer();
    t_paquete* packetHandshake2 = crear_paquete(WORKER_HANDSHAKE, buffer2);
    enviar_paquete(packetHandshake2, w->storage_socket, w->logger);
    eliminar_paquete(packetHandshake2);

    log_info(w->logger, "Estoy justo antes de crear el hilo");
    rtas_storage(w->storage_socket, w);
    log_info(w->logger, "Llegue dsp de recibir a storage");


    //MAster: recibir path de master y ejecuto la query 
        //recibir_path_de_query(w->master_socket, w->logger); Este es el de la funcion anterior de recibir_path_de_query
    
    t_ejecucion* datos_ejecucion = malloc(sizeof(t_ejecucion));
    datos_ejecucion->w = w;
    datos_ejecucion->master_socket = w->master_socket;
    int error_h1 = pthread_create(&ciclo_instrucciones, 
                            NULL, 
                            ejecutar_query,  //recibo el path del query y lo ejecuto
                            datos_ejecucion);
    
    if(error_h1 != 0){
    log_error(w->logger, "No se creo el hilo del ciclo de instrucciones: %s", strerror(error_h1));
    }
    else {
    log_info(w->logger, "Hilo creado correctamente");
    }

    //pthread_join(ciclo_instrucciones, NULL); //Para que el hilo main no termine antes de que el hilo ciclo_instrucciones termine

    //Master: escuchar interrupciones
    t_ejecucion* recibir_interrupciones = malloc(sizeof(t_ejecucion));
    recibir_interrupciones->master_socket = w->master_socket;
    recibir_interrupciones->w = w;
    int error_h2 = pthread_create(&hilo_interrupciones,
                            NULL,
                            hilo_atender_interrupcion,
                            recibir_interrupciones);

    if(error_h2 != 0){
    log_error(w->logger, "No se creo el hilo de las interrupciones: %s", strerror(error_h2));
    }
    else {
    log_info(w->logger, "Hilo de las interrupciones creado correctamente");
    }

    pthread_join(ciclo_instrucciones, NULL);
    pthread_join(hilo_interrupciones, NULL);
    
    //terminar_programa(w->logger, w->config);
    
    return 0;
}