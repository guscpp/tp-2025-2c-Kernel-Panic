#include "storage.h"
#include "operaciones.h"

int main(int argc, char* argv[]) {
    
        if (argc != 2)
        {
            printf("Uso: ./bin/storage [archivo_config]\n");
            return EXIT_FAILURE;
        }
    
    t_storage* storage = iniciar_storage(); // obtiene los configs y los logs
    //verificar_storage(storage);
    inicializar_file_system(storage); // crea el FS si es fresh start

    int storage_fd = iniciar_servidor(storage->puerto_escucha);  //socket, bind, listen    inicia el servidor 
    log_debug(storage->logger, "Servidor listo");

    rutina_recepcion(storage, storage_fd); // acepta conexiones y crea hilos para cada worker que se conecte

    destruir_storage(storage);

    return 0;
}






/////// OJOOOO //////// 
// FALTA EL LOG OBLIGATORIO DE BLOQUE FISICO RESERVADO HAY QUE VER A DONDE PONERLO  //

