#include "worker.h"

int main(int argc, char* argv[]) {
    if (argc != 3)
    {
        printf("Uso: ./bin/worker [archivo_config] [ID Worker]\n");
        return EXIT_FAILURE;
    }

    // ver cuando mandar el ID
    int idworker = atoi(argv[2]);


    t_worker* w = inicializar_worker();
    

    int storage_socket;
    t_buffer* buffer;
    t_paquete* packetHandshake;

    //Para la conexion con storage
    t_buffer* buffer2;
    t_paquete* packetHandshake2;
    //char* log_level_info;

    //Para mandar el ID al master
    t_buffer* buffer3;
    t_paquete* packetID;

    log_info(w->logger, "Verificar funcionamiento logger");

    //Solo logs de prueba: 
    verificar_worker(w);
 
    //Conexion con master
    int master_socket = crear_conexion(w->logger, w->ip_master, w->puerto_master); 
    buffer = crear_buffer();
    packetHandshake = crear_paquete(WORKER_HANDSHAKE, buffer);
    enviar_paquete(packetHandshake, master_socket, w->logger);
    eliminar_paquete(packetHandshake);


    //prueba para checkpoint 1
    //Master: enviarle ID de este worker, recibir path a query
    buffer = crear_buffer();
    t_paquete* paquete = crear_paquete(WORKER_ID, buffer);
    agregar_a_paquete(paquete, &idworker, sizeof(int));
    enviar_paquete(paquete, master_socket, w->logger);
    eliminar_paquete(paquete);
    recibir_path_de_query(master_socket, w->logger);

    //Conexion con Storage
    storage_socket = crear_conexion(w->logger, w->ip_storage, w->puerto_storage); //socket y connect
    buffer2 = crear_buffer();
    packetHandshake2 = crear_paquete(WORKER_HANDSHAKE, buffer2);
    enviar_paquete(packetHandshake2, storage_socket, w->logger);
    eliminar_paquete(packetHandshake2);


    rtas_storage(storage_socket, w);

        //Mandar ID al master
    buffer3 = crear_buffer();
    packetID = crear_paquete(WORKER_ID, buffer3);
    agregar_a_paquete(packetID, &idworker, sizeof(int));
    enviar_paquete(packetID, master_socket, w->logger);
    eliminar_paquete(packetID);
    


    return 0;
}