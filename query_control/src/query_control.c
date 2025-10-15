#include "query_control.h"


t_query_control* inicializar_query_control(int argc, char* argv[])
{
    t_query_control* qc = malloc(sizeof(t_query_control));

    qc->logger        = iniciar_logger("query_control.log", "[QUERY_CONTROL]", true, LOG_LEVEL_INFO);
    qc->config        = iniciar_config(qc->logger, argv[1]);
    qc->ip_master     = config_get_string_value(qc->config, "IP_MASTER");
    qc->puerto_master = config_get_int_value(qc->config, "PUERTO_MASTER");
    qc->log_level     = config_get_string_value(qc->config, "LOG_LEVEL");
    qc->archivo_query = argv[2];
    qc->prioridad     = atoi(argv[3]);
    qc->master_socket = -1; //valor centinela

    return qc;
}


void verificar_query_control(t_query_control* qc)
{
    log_info(qc->logger, "Query Control inicializado correctamente");
    log_info(qc->logger, "IP Master: %s", qc->ip_master);
    log_info(qc->logger, "Puerto Master: %d", qc->puerto_master);
    log_info(qc->logger, "Archivo de query: %s", qc->archivo_query);
    log_info(qc->logger, "Prioridad: %d", qc->prioridad);
    log_info(qc->logger, "Nivel de log: %s", qc->log_level);
}


void liberar_query_control(t_query_control* qc)
{
    if (qc) {
        if (qc->logger) log_destroy(qc->logger);
        if (qc->config) config_destroy(qc->config);
        if (qc->master_socket != -1) close(qc->master_socket);
        free(qc);
    }
}


int conectar_al_master(t_query_control* qc)
{    
    int intentos = 0;
    const int max_intentos = 3;
    const int delay_segundos = 2;
    
    while (intentos < max_intentos) {
        qc->master_socket = crear_conexion(qc->logger, qc->ip_master, string_itoa(qc->puerto_master));
        
        if (qc->master_socket != -1) {
            log_info(qc->logger, "## Conexión al Master exitosa. IP: %s, Puerto: %d", 
                     qc->ip_master, qc->puerto_master);
            return 0;
        }
        
        intentos++;
        if (intentos < max_intentos) {
            log_warning(qc->logger, "Intento %d/%d fallado. Reintentando en %d segundos...", 
                       intentos, max_intentos, delay_segundos);
            sleep(delay_segundos);
        }
    }
    
    log_error(qc->logger, "No se pudo conectar al Master después de %d intentos", max_intentos);
    return -1;
}

void enviar_handshake(t_query_control* qc) {
    t_paquete* paquete = crear_paquete(QC_HANDSHAKE, crear_buffer());
    enviar_paquete(paquete, qc->master_socket, qc->logger);
    eliminar_paquete(paquete);
    log_info(qc->logger, "## HANDSHAKE MASTER");
}

void enviar_path_y_prioridad(t_query_control *qc)
{
    t_buffer  *buffer = crear_buffer();
    t_paquete *paquete = crear_paquete(QUERY_REQUEST, buffer);

    agregar_a_paquete(paquete, qc->archivo_query, strlen(qc->archivo_query) + 1);
    agregar_a_paquete(paquete, &(qc->prioridad), sizeof(int));

    enviar_paquete(paquete, qc->master_socket, qc->logger);
    eliminar_paquete(paquete);

    log_info(qc->logger, "## Envío de query al Master -> Archivo: %s | Prioridad: %d",
        qc->archivo_query, qc->prioridad);
}

void procesar_respuestas_master(t_query_control* qc)
{
    // WHILE (1) esta ok porque recibir_operacion() es bloqueante
    while (1) {
        int codigo_operacion = recibir_operacion(qc->master_socket);
        
        if (codigo_operacion == -1) {
            log_error(qc->logger, "Error en la conexión con el Master");
            break;
        }
        
        switch (codigo_operacion) {
            case QUERY_RESPONSE_READ: {
                int size;
                void* contenido = recibir_buffer(&size, qc->master_socket);
                // el log obligatorio pide esto otro, con file:tag
                // hardcodeo valores y dejo un leak hasta que esto se implemente
                // y Master los envie
                char* file="FILE_HARDCODEADO";
                char* tag ="TAG_HARDCODEADO";
                log_info(qc->logger, "## Lectura realizada: File<%s:%s>, contenido:%s", 
                    file, tag, (char*)contenido);
                free(contenido);
                break;
            }
            case QUERY_RESPONSE_END: {
                int size;
                void* motivo = recibir_buffer(&size, qc->master_socket);
                log_info(qc->logger, "## Query Finalizada - %s", (char*)motivo);
                free(motivo);
                return;
            }
            case QUERY_RESPONSE_ERROR: {
                int size;
                void* error = recibir_buffer(&size, qc->master_socket);
                log_error(qc->logger, "## Query Finalizada - Error: %s", (char*)error);
                free(error);
                return;
            }
            default:
                log_warning(qc->logger, "Código de operación desconocido: %d", codigo_operacion);
                break;
        }
    }
}
