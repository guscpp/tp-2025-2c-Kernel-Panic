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
    log_info(storage->logger, "Servidor listo");

    rutina_recepcion(storage, storage_fd); // acepta conexiones y crea hilos para cada worker que se conecte

    destruir_storage(storage);

    return 0;
}



// para el yo del futuro: el archivo block_hash_index.config para asociar cada bloque fisico con un hash tiene que ser dentro de recrear_hash?
// como se agrega los bloques fisicos al physical_blocks? por ejemplo si quiero agregar block00001.dat y asi sucesivamente, significa que el tamanio del file system indica la cantidad de bloques que necesita ?
// los contenidos del archivo config son constantes o pueden cambiar en tiempo de ejecucion? por ejemplo el tamanio del file system puede cambiar en tiempo de ejecucion? o el tamanio del bloque?, para saber si usar config_get_int_value o guardarlos en variables al iniciar el storage