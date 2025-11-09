#include "master.h"

char* algoritmo_planificacion ;
int tiempo_aging;

int main(int argc, char* argv[]) {
    
    if (argc != 2)
    {
        printf("Uso: ./bin/master [archivo_config]\n");
        return EXIT_FAILURE;
    }
    
    //comento las variables que no se usan para callar warnings
    //int conexion;
	//char* ip;
	char* puerto;
    //t_log_level log_level;
    t_log* logger;
	t_config* config;
    int err;        // por si algo falla
     
                              // tipo de dato que cree para pasar a la funcion de atender 
                              // conexion porque los hilos funcionan como quieren
    
    logger = iniciar_logger("master.log","MASTER",true, LOG_LEVEL_INFO);
    config = iniciar_config(logger,argv[1]);
    puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    algoritmo_planificacion = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    log_info(logger, "ALGORITMO: %s" , algoritmo_planificacion);
    tiempo_aging = config_get_int_value(config,"TIEMPO_AGING");

    int master_fd = iniciar_servidor(puerto);
    log_info(logger, "Servidor listo para recibir una conexion - FD: %i / puerto: %s" , master_fd, puerto);
     
    inicializar_semaforos(logger);
     
    while (1) {

        t_hacerConnect* datosConexion = malloc(sizeof(t_hacerConnect));
        datosConexion->logger = logger;

        pthread_t thread;

        int fd_conexion_master = esperar_cliente(master_fd);
        log_info(logger, "Esperando que se conecte un cliente");

        datosConexion->socket_conexion = fd_conexion_master;

        err= pthread_create(&thread,
                        NULL,
                        atender_conexion, // va a llamar a atender conexion pasando parametros
                        datosConexion); // de este struct
        if (err != 0){
            log_info(logger, "Hubo un problema al crear el hilo");
        }
        pthread_detach(thread);
        
    }

    terminar_programa(logger,config);
    return 0;

}


