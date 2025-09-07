#include "cliente.h"

int main(int argc, char* argv[]) {

    t_log* logger;
    t_log* loggerOficial;
    char* ip_master;
    int puerto_master;
    char* ip_storage;
    int puerto_storage;
    int tam_memoria;
    int retardo_memoria;
    char* algoritmo_reemplazo;
    char* path_scripts;

    int storage_socket;
    t_buffer* buffer;
    t_paquete* packetHandshke;
    //char* log_level_info;


    logger = iniciar_logger("worker.log", "[WORKER_PRUEBA]", true, LOG_LEVEL_INFO); //al final pasarlo a false
    loggerOficial = iniciar_logger("worker.log", "[WORKER]", true, LOG_LEVEL_INFO);
    log_info(logger, "Verificar funcionamineto logger");

    t_config* config = iniciar_config(logger, "worker.config");

    ip_master = config_get_string_value(config, "IP_MASTER");
    puerto_master = config_get_int_value(config, "PUERTO_MASTER");
    ip_storage = config_get_string_value(config, "IP_STORAGE");
    puerto_storage = config_get_int_value(config, "PUERTO_STORAGE");
    tam_memoria = config_get_int_value(config, "TAM_MEMORIA");
    retardo_memoria = config_get_int_value(config, "RETARDO_MEMORIA");
    algoritmo_reemplazo = config_get_string_value(config, "ALGORITMO_REEMPLAZO");
    path_scripts = config_get_string_value(config, "PATH_SCRIPTS");

    //Solo logs de prueba: 
    log_info(logger, "Ip_Master: %s", ip_master);
    log_info(logger, "Puerto Master: %d", puerto_master);
    log_info(logger, "Ip_Storage: %s", ip_storage);
    log_info(logger, "Puerto_Storage: %d", puerto_storage);
    log_info(logger, "Tam_memoria: %d", tam_memoria);
    log_info(logger, "Retardo_Memoria: %d", retardo_memoria);
    log_info(logger, "Algoritmos_reemplazo: %s", algoritmo_reemplazo);
    log_info(logger, "PathScript: %s", path_scripts);
    

    //Conexion con Storage
    /*
    storage_socket = crear_conexion(logger, ip_storage, puerto_storage); 
    buffer = crear_buffer();
    packetHandshke = crear_paquete(HANDSHAKE_CPU, buffer);

    add_to_packet(packetHandshke, buffer->stream, buffer->size);
    enviar_paquete(packetHandshke, storage_socket);
    eliminarPaquete(packetHandshke);
    */

    return 0;
}
