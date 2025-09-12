#include "master.h"

int main(int argc, char* argv[]) {
    
    int conexion;
	char* ip;
	char* puerto;

    t_log* logger;
	t_config* config;

    logger = iniciar_logger("master.log","MASTER",true, LOG_LEVEL_INFO);

    config = iniciar_config(logger,"master.config");
    
    puerto = config_get_string_value(config, "PUERTO_ESCUCHA");

    int master_fd = iniciar_servidor(puerto);
    log_info(logger, "Servidor listo para recibir al cliente");
    
    int query_fd= esperar_cliente(master_fd);

    




    terminar_programa(logger,config);
}

