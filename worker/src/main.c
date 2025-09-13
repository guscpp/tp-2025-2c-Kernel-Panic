#include "worker.h"

int main(int argc, char* argv[]) {
    if (argc != 3)
    {
        printf("Uso: ./bin/worker [archivo_config] [ID Worker]\n");
        return EXIT_FAILURE;
    }

    t_worker* w = inicializar_worker();

    // FALTA algo como esto
    //int IDWORKER = argv[2];

    int storage_socket;
    t_buffer* buffer;
    t_paquete* packetHandshake;

    //Para la conexion con storage
    t_buffer* buffer2;
    t_paquete* packetHandshake2;
    //char* log_level_info;

    log_info(w->logger, "Verificar funcionamiento logger");

    //Solo logs de prueba: 
    verificar_worker(w);
 
    //Conexion con master
    int master_socket = crear_conexion(w->logger, w->ip_master, w->puerto_master); //socket y connect
    buffer = crear_buffer();
    packetHandshake = crear_paquete(WORKER_HANDSHAKE, buffer);

    agregar_a_paquete(packetHandshake, buffer-> stream, buffer->size);
    enviar_paquete(packetHandshake, master_socket, w->logger);
    eliminar_paquete(packetHandshake);

    
    //Conexion con Storage
    storage_socket = crear_conexion(w->logger, w->ip_storage, w->puerto_storage); //socket y connect
    buffer2 = crear_buffer();
    packetHandshake2 = crear_paquete(WORKER_HANDSHAKE, buffer2);

    agregar_a_paquete(packetHandshake2, buffer2-> stream, buffer2->size);
    enviar_paquete(packetHandshake2, storage_socket, w->logger);
    eliminar_paquete(packetHandshake2);
    

    return 0;
}