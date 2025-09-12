#include "storage.h"

int main(int argc, char* argv[]) {
    
    t_storage* storage = iniciar_storage();

    log_info(storage->logger, "Puerto leido: %s", storage->puerto);
    log_info(storage->logger, "Fresh leido: %d", storage->fresh);
    log_info(storage->logger, "Punto de montaje leido: %s", storage->punto_montaje);
    log_info(storage->logger, "Retardo operacion leido: %d", storage->retardo_operacion);
    log_info(storage->logger, "Retardo acceso bloque leido: %d", storage->retardo_acceso_bloque);
    log_info(storage->logger, "log level leido: %s", storage->log_level);

    
    int storage_fd = iniciar_servidor(storage->puerto);  //socket, bind, listen
    log_info(storage->logger, "Servidor listo");
    int worker_fd = esperar_cliente(storage_fd);//acept
    log_info(storage->logger, "Llego un cliente"); 
    
    liberar_storage(storage);
    
    return 0;
}
