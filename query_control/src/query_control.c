#include "query_control.h"


// ****************************************************************************
t_query_control* inicializar_query_control(int argc, char* argv[])
{
    t_query_control* qc = malloc(sizeof(t_query_control));

    // Logger temporal para inicialización
    t_log* logger_temp = iniciar_logger("query_control.log", "[QUERY_CONTROL_INIT]", true, LOG_LEVEL_INFO);

    qc->config = iniciar_config(logger_temp, argv[1]);

    // Obtener nivel real del config
    //qc->log_level = config_get_string_value(qc->config, "LOG_LEVEL");
    qc->log_level = strdup(config_get_string_value(qc->config, "LOG_LEVEL"));    
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


// ****************************************************************************
void verificar_query_control(t_query_control* qc)
{
    log_debug(qc->logger, "** Query Control inicializado correctamente");
    log_debug(qc->logger, "** IP Master: %s", qc->ip_master);
    log_debug(qc->logger, "** Puerto Master: %d", qc->puerto_master);
    log_debug(qc->logger, "** Archivo de query: %s", qc->archivo_query);
    log_debug(qc->logger, "** Prioridad: %d", qc->prioridad);
}


// ****************************************************************************
void liberar_query_control(t_query_control* qc)
{
    if (!qc) return;

    if (qc->logger) log_destroy(qc->logger);
    if (qc->ip_master) free(qc->ip_master);
    if (qc->log_level) free(qc->log_level);
    if (qc->config) config_destroy(qc->config);
    if (qc->master_socket != -1) close(qc->master_socket);
    free(qc);
}


// ****************************************************************************
int conectar_al_master(t_query_control* qc)
{    
    
    char* puerto_str = string_itoa(qc->puerto_master);
    qc->master_socket = crear_conexion(qc->logger, qc->ip_master, puerto_str);
    free(puerto_str);
    
    if (qc->master_socket != -1) {
        log_info(qc->logger, COLOR_VERDE "## Conexión al Master exitosa. IP: %s, Puerto: %d" COLOR_VERDE, qc->ip_master, qc->puerto_master);
        return 0;

    }else{
        log_error(qc->logger, "** Error al Conectar al Master");
        return -1; 
    }
}


// ****************************************************************************
void enviar_handshake(t_query_control* qc) {
    t_paquete* paquete = crear_paquete(QC_HANDSHAKE, crear_buffer());
    enviar_paquete(paquete, qc->master_socket, qc->logger);
    eliminar_paquete(paquete);
    log_debug(qc->logger, "** HANDSHAKE MASTER");
}


// ****************************************************************************
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
        log_info(qc->logger,"## Solicitud de ejecución de Query: %s, prioridad: %d", nombre_archivo, qc->prioridad);
    } else {
        log_error(qc->logger, "** Error al enviar query al Master");
    }
    
    eliminar_paquete(paquete);
}


// ****************************************************************************
void procesar_respuestas_master(t_query_control* qc)
{    
    while (1) {
        log_debug(qc->logger, "** Esperando respuesta del Master...");
        
        t_list* paqueteMaster = recibir_paquete(qc->master_socket);
        
        // Verificar si el paquete es NULL (conexión cerrada)
        if (paqueteMaster == NULL) {
            log_info(qc->logger,"## Query Finalizada - Query Desconectada de Master");
            break;
        }
        
        int* codigo_operacion_ptr = list_get(paqueteMaster, 0);
        if (codigo_operacion_ptr == NULL) {
            log_info(qc->logger, "** Error: código de operación nulo");
            list_destroy(paqueteMaster);
            continue;
        }

        int codigo_operacion = *codigo_operacion_ptr;
        log_debug(qc->logger, "** Código de operación recibido: %d", codigo_operacion);
        
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


            case QUERY_RESPONSE_ERROR_ARCHIVO_NO_ENCONTRADO: {
                

               // char* file = (char*)list_get(paqueteMaster, 1);
               // char* tag = (char*)list_get(paqueteMaster, 2);
    
                log_info(qc->logger, "## Query Finalizada - Error: Archivo No Encontrado");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            
            case QUERY_RESPONSE_ERROR_ARCHIVO_QUERY_NO_ENCONTRADO: {
                

              //  char* path = (char*)list_get(paqueteMaster, 1);
    
                log_info(qc->logger,"## Query Finalizada - Error: Path de Query No Encontrado");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            case QUERY_RESPONSE_ERROR_ERROR_EN_INSTRUCCION: {

                char* instruccion = (char*)list_get(paqueteMaster, 1);

                log_info(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Error Al Ejecutar la Instrucción: %s", instruccion);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

    

            case QUERY_RESPONSE_ERROR_WORKER_DESCONECTADO: {
                
                int worker_id = *(int*)list_get(paqueteMaster, 1);

                log_info(qc->logger, "## Query Finalizada - Error: Worker con ID : %d Desconectado", worker_id);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }

            case QUERY_RESPONSE_ERROR_MODIFICAR_COMMIT: {
                
               // char* file = (char*)list_get(paqueteMaster, 1);
               // char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: Se Intento Modifical al Archivo que se Encuentra en estado de COMMITED");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }

            case QUERY_RESPONSE_ERROR_TAMANIO_ESCRITURA_EXCEDIDO: {
                
               // char* file = (char*)list_get(paqueteMaster, 1);
               // char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: Tamaño de Escritura Excedido ");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }

            case QUERY_RESPONSE_ERROR_TAMANIO_LECTURA_EXCEDIDO: {
                
               // char* file = (char*)list_get(paqueteMaster, 1);
               // char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: Lectura fuera de limite ");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }
             case QUERY_ERROR_CREATE: {
                
               // char* file = (char*)list_get(paqueteMaster, 1);
               // char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: No se pudo crear el archivo ");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            } case QUERY_ERROR_TRUNCATE: {
                
                //char* file = (char*)list_get(paqueteMaster, 1);
                //char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: No se pudo realizar el TRUNCATE ");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }
             case QUERY_ERROR_WRITE_EN_STORAGE: {
                
              //  char* file = (char*)list_get(paqueteMaster, 1);
               // char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: No se pudo realizar la escritura en Storage ");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }
            case QUERY_ERROR_READ_EN_STORAGE: {
                
                //char* file = (char*)list_get(paqueteMaster, 1);
                //char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: No se pudo realizar la lectura en Storage ");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }
            case QUERY_ERROR_TAG: {
                
                //char* file = (char*)list_get(paqueteMaster, 1);
                //char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: No se pudo realizar el TAG ");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }
            case QUERY_ERROR_COMMIT: {
                
                //char* file = (char*)list_get(paqueteMaster, 1);
                //char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: No se pudo realizar el COMMIT ");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }
            case QUERY_ERROR_DELETE: {
                
                //char* file = (char*)list_get(paqueteMaster, 1);
                //char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: No se pudo realizar el DELETE ");
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;

            }
            case QUERY_ERROR_FLUSH: {
                
               // char* file = (char*)list_get(paqueteMaster, 1);
                //char* tag = (char*)list_get(paqueteMaster, 2);

                log_info(qc->logger, "## Query Finalizada - Error: No se pudo realizar el FLUSH ");
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