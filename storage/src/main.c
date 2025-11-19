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

    // rutina_recepcion(storage, storage_fd); // acepta conexiones y crea hilos para cada worker que se conecte

    
    t_list* parametros = list_create();
    int codigo_operacion = STORAGE_CREATE_FILE;
    int query_id = 1;
    char* file = "testfile";
    char* tag = "tag1";
    list_add(parametros, &codigo_operacion);
    list_add(parametros, &query_id);
    list_add(parametros, file);
    list_add(parametros, tag);

    
    crear_file(storage, parametros);
    log_info(storage->logger, "CREARRRRRRRRRRRRRRRR");

    t_list* parametros_truncar = list_create();
    codigo_operacion = STORAGE_TRUNCATE;
    query_id = 2;
    int tamanio = 256;
    list_add(parametros_truncar, &codigo_operacion);
    list_add(parametros_truncar, &query_id);
    list_add(parametros_truncar, file);
    list_add(parametros_truncar, tag);
    list_add(parametros_truncar, &tamanio);
    truncar_file(storage, parametros_truncar);
    log_info(storage->logger, "TRUNCARRRRRRRRRRRRRRR");

    escribir_bloque_test(storage, file, tag, 0, "AAAAAAAAAAAA");
    escribir_bloque_test(storage, file, tag, 1, "BBBBBBBBBBBB");


    t_list* parametros_commit = list_create(); 
    codigo_operacion = STORAGE_COMMIT;
    query_id = 3;
    list_add(parametros_commit, &codigo_operacion);
    list_add(parametros_commit, &query_id);
    list_add(parametros_commit, file);
    list_add(parametros_commit, tag);


    realizar_commit(storage, parametros_commit);
    log_info(storage->logger, "COMITEARRRRRRRRRRRRRRR");

    destruir_storage(storage);

    return 0;
}

