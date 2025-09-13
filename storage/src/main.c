#include "storage.h"

int main(int argc, char* argv[]) {
        if (argc != 2)
        {
            printf("Uso: ./bin/storage [archivo_config]\n");
            return EXIT_FAILURE;
        }

    t_storage* storage = iniciar_storage();

    verificar_storage(storage);

    int storage_fd = iniciar_servidor(storage->puerto_escucha);  //socket, bind, listen
    log_info(storage->logger, "Servidor listo");
    int worker_fd = esperar_cliente(storage_fd);//acept
    log_info(storage->logger, "Llego un cliente");   
    
    enviar_tamanio_paquete_aworker(worker_fd, storage->logger);
    liberar_storage(storage);
    
    return 0;
}
