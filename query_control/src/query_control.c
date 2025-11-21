#include "query_control.h"


t_query_control* inicializar_query_control(int argc, char* argv[])
{
    t_query_control* qc = malloc(sizeof(t_query_control));

    // Logger temporal para inicialización
    t_log* logger_temp = iniciar_logger("query_control.log", "[QUERY_CONTROL_INIT]", true, LOG_LEVEL_INFO);

    qc->config = iniciar_config(logger_temp, argv[1]);

    // Obtener nivel real del config
    qc->log_level = config_get_string_value(qc->config, "LOG_LEVEL");
    t_log_level nivel = obtener_log_level(qc->log_level);

    // Crear logger final con el nivel real
    qc->logger = iniciar_logger("query_control.log", "[QUERY_CONTROL]", true, nivel);

    // Ya puedo destruir el logger temporal
    log_destroy(logger_temp);

    qc->ip_master     = strdup(config_get_string_value(qc->config, "IP_MASTER"));
    qc->puerto_master = config_get_int_value(qc->config, "PUERTO_MASTER");
    qc->archivo_query = argv[2];
    qc->prioridad     = atoi(argv[3]);
    qc->master_socket = -1;

    return qc;
}



void verificar_query_control(t_query_control* qc)
{
    log_info(qc->logger, "** Query Control inicializado correctamente");
    log_info(qc->logger, "** IP Master: %s", qc->ip_master);
    log_info(qc->logger, "** Puerto Master: %d", qc->puerto_master);
    log_info(qc->logger, "** Archivo de query: %s", qc->archivo_query);
    log_info(qc->logger, "** Prioridad: %d", qc->prioridad);
    log_info(qc->logger, "** Nivel de log: %s", qc->log_level);
    log_debug(qc->logger, "** Nivel de log debug anda ");
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
    const int max_intentos = 5;
    const int delay_segundos = 2;
    
    while (intentos < max_intentos) {
        log_info(qc->logger, "** Intentando conectar al Master (intento %d/%d)...", 
                intentos + 1, max_intentos);
        
        qc->master_socket = crear_conexion(qc->logger, qc->ip_master, string_itoa(qc->puerto_master));
        
        if (qc->master_socket != -1) {
            log_info(qc->logger,COLOR_VERDE "## Conexión al Master exitosa. IP: %s, Puerto: %d" COLOR_VERDE, 
                     qc->ip_master, qc->puerto_master);
            return 0;
        }
        
        intentos++;
        if (intentos < max_intentos) {
            log_warning(qc->logger, "** Intento %d/%d fallado. Reintentando en %d segundos...", 
                       intentos, max_intentos, delay_segundos);
            sleep(delay_segundos);
        }
    }
    
    log_error(qc->logger, "** No se pudo conectar al Master después de %d intentos", max_intentos);
    return -1;
}

void enviar_handshake(t_query_control* qc) {
    t_paquete* paquete = crear_paquete(QC_HANDSHAKE, crear_buffer());
    enviar_paquete(paquete, qc->master_socket, qc->logger);
    eliminar_paquete(paquete);
    log_info(qc->logger, "** HANDSHAKE MASTER");
}
void enviar_path_y_prioridad(t_query_control *qc)
{
    t_buffer *buffer = crear_buffer();
    t_paquete *paquete = crear_paquete(QUERY_REQUEST, buffer);

    // Enviar nombre del archivo (sin la ruta completa si es posible)
    char* nombre_archivo = strrchr(qc->archivo_query, '/');
    if (nombre_archivo == NULL) {
        nombre_archivo = qc->archivo_query;
    } else {
        nombre_archivo++; // Saltar el '/'
    }
    
    agregar_a_paquete(paquete, nombre_archivo, strlen(nombre_archivo) + 1);
    agregar_a_paquete(paquete, &(qc->prioridad), sizeof(int));

    int resultado = enviar_paquete(paquete, qc->master_socket, qc->logger);
    
    if (resultado == 0) {
        log_info(qc->logger,COLOR_VERDE "## Solicitud de ejecución de Query: %s, prioridad: %d" COLOR_VERDE , 
                nombre_archivo, qc->prioridad);
    } else {
        log_error(qc->logger, "** Error al enviar query al Master");
    }
    
    eliminar_paquete(paquete);
}

void procesar_respuestas_master(t_query_control* qc)
{    
    while (1) {
        log_info(qc->logger, "** Esperando respuesta del Master...");
        
        t_list* paqueteMaster = recibir_paquete(qc->master_socket);
        
        // Verificar si el paquete es NULL (conexión cerrada)
        if (paqueteMaster == NULL) {
            log_info(qc->logger, COLOR_VERDE"## Query Finalizada - Query Desconectada de Master" COLOR_VERDE);
            break;
        }
        
        int* codigo_operacion_ptr = list_get(paqueteMaster, 0);
        if (codigo_operacion_ptr == NULL) {
            log_error(qc->logger, "** Error: código de operación nulo");
            list_destroy(paqueteMaster);
            continue;
        }
        
        int codigo_operacion = *codigo_operacion_ptr;
        log_info(qc->logger, "** Código de operación recibido: %d", codigo_operacion);

        switch (codigo_operacion) {

            case QUERY_RESPONSE_READ: {
                char* file = (char*)list_get(paqueteMaster, 1);
                char* tag = (char*)list_get(paqueteMaster, 2);
                char* contenido = (char*)list_get(paqueteMaster, 3);
                
                log_info(qc->logger, COLOR_VERDE "## Lectura realizada: File %s:%s, contenido: %s" COLOR_VERDE, file, tag, contenido);
                break;
            }

            case QUERY_RESPONSE_END: {
                log_info(qc->logger, COLOR_VERDE "## Query Finalizada - Query Finalizada Con Éxito" COLOR_VERDE);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            case QUERY_RESPONSE_ERROR: {
                log_info(qc->logger, COLOR_VERDE "## Query Finalizada - Error: No Especificado" COLOR_VERDE);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            case QUERY_RESPONSE_ERROR_ARCHIVO_NO_ENCONTRADO: {
                

                char* file = (char*)list_get(paqueteMaster, 1);
                char* tag = (char*)list_get(paqueteMaster, 2);
    
                log_info(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Archivo %s:%s No Encontrado" COLOR_VERDE, file, tag);

                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            
            case QUERY_RESPONSE_ERROR_ARCHIVO_QUERY_NO_ENCONTRADO: {
                

                char* path = (char*)list_get(paqueteMaster, 1);
    
                log_info(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Archivo de Query : %s No Encontrado" COLOR_VERDE, path);

                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            case QUERY_RESPONSE_ERROR_ERROR_EN_INSTRUCCION: {

                char* instruccion = (char*)list_get(paqueteMaster, 1);
                log_info(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Error Al Ejecutar la Instrucción: %s" COLOR_VERDE, instruccion);

                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            case QUERY_RESPONSE_ERROR_LECTURA_INVALIDA: {
                
                char* file = (char*)list_get(paqueteMaster, 1);
                char* tag = (char*)list_get(paqueteMaster, 2);
                log_info(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Lectura Inválida del Archivo : %s:%s" COLOR_VERDE, file, tag);

                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            case QUERY_RESPONSE_ERROR_WORKER_DESCONECTADO: {
                
                char* worker_id = (char*)list_get(paqueteMaster, 1);
                log_error(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Worker con ID : %s Desconectado" COLOR_VERDE, worker_id);

                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            case QUERY_RESPONSE_ERROR_MODIFICAR_COMMIT: {
                
                char* file = (char*)list_get(paqueteMaster, 1);
                char* tag = (char*)list_get(paqueteMaster, 2);
                log_error(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Se Intento Modifical al Archivo %s:%s que se Encuentra en estado de COMMITED" COLOR_VERDE, file, tag);

                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }

            case QUERY_RESPONSE_ERROR_TAMANIO_ESCRITURA_EXCEDIDO: {
                
                char* file = (char*)list_get(paqueteMaster, 1);
                char* tag = (char*)list_get(paqueteMaster, 2);
                log_error(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Tamaño de Escritura Excedido Para el Archivo %s:%s" COLOR_VERDE, file, tag);

                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }

            case QUERY_RESPONSE_ERROR_TAMANIO_LECTURA_EXCEDIDO: {
                
                char* file = (char*)list_get(paqueteMaster, 1);
                char* tag = (char*)list_get(paqueteMaster, 2);
                log_error(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Tamaño de Lectura Excedido Para el Archivo %s:%s" COLOR_VERDE, file, tag);

                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }

            case QUERY_RESPONSE_ERROR_STORAGE_DESCONECTADO: {
                
                log_error(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Storage Desconectado" COLOR_VERDE);

                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }

            default: {
                log_warning(qc->logger, "Código de operación desconocido: %d", codigo_operacion);
                break;
            }
        }

        list_destroy_and_destroy_elements(paqueteMaster, free);
    }
}

