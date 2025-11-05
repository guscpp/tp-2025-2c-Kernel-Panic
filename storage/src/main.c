#include "storage.h"
#include "operaciones.h"

bool crear_file(t_storage* storage, t_list* parametros); // prototipo para test

int main(int argc, char* argv[]) {
    
        if (argc != 2)
        {
            printf("Uso: ./bin/storage [archivo_config]\n");
            return EXIT_FAILURE;
        }
    
    t_storage* storage = iniciar_storage(); // obtiene los configs y los logs
    //verificar_storage(storage);
    inicializar_file_system(storage); // crea el FS si es fresh start

// ========== PRUEBA AISLADA DE LEER_BLOQUE ==========
printf("\n🧪 Probando LEER_BLOQUE de forma aislada...\n");
t_list* paquete_read = list_create();
int opcode = STORAGE_READ_BLOCK;
int query_id = 999;
char* file = "initial_file";
char* tag = "BASE";
int bloque = 0;
list_add(paquete_read, &opcode);
list_add(paquete_read, &query_id);
list_add(paquete_read, file);
list_add(paquete_read, tag);
list_add(paquete_read, &bloque);

void* contenido = NULL;
int tam_bloque = 0;
printf("Ingresando a leer_bloque() ... \n");
if (leer_bloque(storage, paquete_read, &contenido, &tam_bloque)) {
    printf("✅ Lectura exitosa. Contenido (primeros 20 bytes): '");
    fwrite(contenido, 1, tam_bloque > 20 ? 20 : tam_bloque, stdout);
    printf("'\n");
    free(contenido);
} else {
    printf("❌ Falló leer_bloque\n");
}
list_destroy(paquete_read);
// ========== FIN PRUEBA ==========

    int storage_fd = iniciar_servidor(storage->puerto_escucha);  //socket, bind, listen    inicia el servidor 
    log_info(storage->logger, "Servidor listo");

    rutina_recepcion(storage, storage_fd); // acepta conexiones y crea hilos para cada worker que se conecte

    return 0;

}



// para el yo del futuro: el archivo block_hash_index.config para asociar cada bloque fisico con un hash tiene que ser dentro de recrear_hash?
// como se agrega los bloques fisicos al physical_blocks? por ejemplo si quiero agregar block00001.dat y asi sucesivamente, significa que el tamanio del file system indica la cantidad de bloques que necesita ?
// los contenidos del archivo config son constantes o pueden cambiar en tiempo de ejecucion? por ejemplo el tamanio del file system puede cambiar en tiempo de ejecucion? o el tamanio del bloque?, para saber si usar config_get_int_value o guardarlos en variables al iniciar el storage