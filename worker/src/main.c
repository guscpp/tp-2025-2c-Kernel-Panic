#include "../include/worker.h"

int main(int argc, char* argv[]) {
//Lo comente para poder hacer debug de lo ultimo que agregue



    if (argc != 3)
    {
        printf("Uso: ./bin/worker [archivo_config] [ID Worker]\n");
        return EXIT_FAILURE;
    }

    int id_worker = atoi(argv[2]);

    //int id_worker = 2; //id hardcodeado (en vez de venir por consola)



    t_worker* w = inicializar_worker(id_worker);
    
    t_query_interpreter*  query_interpreter =  query_interpreter_crear(w->logger); //tiene pc y un verificador de interrupciones
    w->interpreter = query_interpreter; 

    pthread_t ciclo_instrucciones; 
    pthread_t hilo_interrupciones;
    log_info(w->logger, "Verificar funcionamiento logger");

    //Solo logs de prueba: 
    verificar_worker(w);
    semaforos(w);
 
    //Master: crear la conexion para distpacher (primer connecy)
    w->master_socket_distpach = crear_conexion(w->logger, w->ip_master, string_itoa(w->puerto_master)); 
    t_buffer* buffer1 = crear_buffer();
    t_paquete* packetHandshake = crear_paquete(WORKER_HANDSHAKE, buffer1);
    enviar_paquete(packetHandshake, w->master_socket_distpach, w->logger);
    eliminar_paquete(packetHandshake);

    //Master: enviarle ID de este worker
    buffer1 = crear_buffer();
    t_paquete* packetID = crear_paquete(WORKER_ID, buffer1);
    agregar_a_paquete(packetID, &w->id_worker, sizeof(int));
    enviar_paquete(packetID, w->master_socket_distpach, w->logger);
    eliminar_paquete(packetID);

    retener_worker(w); 

    //MAster: crear la conexion para el interrupt (segundp connect)
    w->master_socket_interrupt = crear_conexion(w->logger, w->ip_master, string_itoa(w->puerto_master));

    //MAster: enviar id para el interrupt 
    t_buffer* buffer5 = crear_buffer();
    t_paquete* packetID_interrupt = crear_paquete(WORKER_ID_INTERRUPT, buffer5); //este es el hanshake del segundo connect
    agregar_a_paquete(packetID_interrupt, &w->id_worker, sizeof(int));
    enviar_paquete(packetID_interrupt, w->master_socket_interrupt, w->logger);
    eliminar_paquete(packetID_interrupt);
    
    //Storge: crear la conexion
    w->storage_socket = crear_conexion(w->logger, w->ip_storage, w->puerto_storage); //socket y connect
    t_buffer* buffer2 = crear_buffer();
    t_paquete* packetHandshake2 = crear_paquete(WORKER_HANDSHAKE, buffer2);
    //int a = 12;
    //agregar_a_paquete(packetHandshake2, &a, sizeof(int));
    enviar_paquete(packetHandshake2, w->storage_socket, w->logger);
    log_info(w->logger, "Acabo de enviar WORKER_HANDSHAKE a Storage");
    eliminar_paquete(packetHandshake2);

    //Storage: pedir el tamanio de bloque
    t_buffer* buffer3 = crear_buffer();
    t_paquete* paquete_tamanio_bloque = crear_paquete(STORAGE_GET_BLOCK_SIZE, buffer3);
    ///agregar_a_paquete(paquete_tamanio_bloque, &a, sizeof(int));
    enviar_paquete(paquete_tamanio_bloque, w->storage_socket, w->logger);
    //enviar_paquete(paquete_tamanio_bloque, w->storage_socket, w->logger); // enviarlo 2 veces :|
    log_info(w->logger, "Acabo de enviar STORAGE_GET_BLOCK_SIZE a Storage");
    eliminar_paquete(paquete_tamanio_bloque);

    //Respuestas de storage
    rtas_storage(w->storage_socket, w);
    log_info(w->logger, "Llegue dsp de recibir a storage");

    t_memoria_interna* m = crear_memoria(
        w->logger,
        config_get_int_value(w->config, "TAM_MEMORIA"),
        config_get_int_value(w->config, "RETARDO_MEMORIA"),
        config_get_string_value(w->config, "ALGORITMO_REEMPLAZO"),
        w->tamanio_bloque
    );
    w->mem = m;  
    m->socket_storage = w->storage_socket;  
    m->tmp_bloque = NULL;

    //MAster: escuchar querys
    t_ejecucion* datos_ejecucion = malloc(sizeof(t_ejecucion));
    datos_ejecucion->w = w;
    datos_ejecucion->master_socket = w->master_socket_distpach; //el datos_ejecucion tiene el nombre de master_socket pero guarda el de dispatch, porque sino corria peligro de no cambiarlo en algun lugar
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


    //Master: escuchar interrupciones
    t_ejecucion* recibir_interrupciones = malloc(sizeof(t_ejecucion));
    recibir_interrupciones->master_socket = w->master_socket_interrupt; //RECORDAR que recibir_interrupciones tiene master_socket pero se refiere al socket del interrupt
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
    
    return 0;
}