#include "master.h"

int main(int argc, char* argv[]) {
    
    int conexion;
	char* ip;
	char* puerto;
    t_log_level log_level;
    t_log* logger;
	t_config* config;
    int err;        // por si algo falla
    hacerConect* datosConexion; // tipo de dato que cree para pasar a la funcion de atender 
                              // conexion porque los hilos funcionan como quieren
    

    logger = iniciar_logger("master.log","MASTER",true, LOG_LEVEL_INFO);
    config = iniciar_config(logger,"master.config");
    puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
    datosConexion->logger = logger; 
    
    int master_fd = iniciar_servidor(puerto);
    log_info(logger, "Servidor listo para recibir una conexion");
   
   
    while (1) {
        pthread_t thread;
        int *fd_conexion_master = malloc(sizeof(int));
        *fd_conexion_master = esperar_cliente(master_fd);

        datosConexion->socket_conexion = *fd_conexion_master;

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


