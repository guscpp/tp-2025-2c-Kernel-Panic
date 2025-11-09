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
    log_debug(qc->logger, "** Query Control inicializado correctamente");
    log_info(qc->logger, "** IP Master: %s", qc->ip_master);
    log_info(qc->logger, "** Puerto Master: %d", qc->puerto_master);
    log_info(qc->logger, "** Archivo de query: %s", qc->archivo_query);
    log_info(qc->logger, "** Prioridad: %d", qc->prioridad);
    log_info(qc->logger, "** Nivel de log: %s", qc->log_level);
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
            log_error(qc->logger, "Conexión con el Master fue cerrada"); // revisar
            break;
        }
        
        // Verificar que la lista tenga elementos
        if (list_is_empty(paqueteMaster)) {
            log_warning(qc->logger, "** Paquete vacío recibido del Master");
            list_destroy(paqueteMaster);
            continue;
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
                // Verificar que tenemos suficientes elementos
                if (list_size(paqueteMaster) >= 4) {
                    char* file = (char*)list_get(paqueteMaster, 1);
                    char* tag = (char*)list_get(paqueteMaster, 2);
                    char* contenido = (char*)list_get(paqueteMaster, 3);
                    
                    if (file && tag && contenido) {
                        log_info(qc->logger,COLOR_VERDE "## Lectura realizada: File %s:%s, contenido: %s" COLOR_VERDE, 
                                file, tag, contenido);
                    } else {
                        log_error(qc->logger, "** Datos incompletos en respuesta de lectura");
                    }
                } else {
                    log_error(qc->logger, "** Paquete de lectura incompleto");
                }
                break;
            }
            
            case QUERY_RESPONSE_END: { //revisar
                if (list_size(paqueteMaster) >= 2) {
                    char* motivo = (char*)list_get(paqueteMaster, 1);
                    log_info(qc->logger,COLOR_VERDE "## Query Finalizada - %s", motivo ? motivo : "Query Finalizada Con Éxito" COLOR_VERDE);
                } else {
                    log_info(qc->logger, "## Query Finalizada");
            
                }
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
            }
            
            case QUERY_RESPONSE_ERROR:
                log_error(qc->logger, COLOR_VERDE "## Query Finalizada - Error: No Especificado" COLOR_VERDE);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
                
            case QUERY_RESPONSE_ERROR_ARCHIVO_NO_ENCONTRADO:
                log_error(qc->logger, COLOR_VERDE"## Query Finalizada - Error: Archivo No Encontrado" COLOR_VERDE);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
                
            case QUERY_RESPONSE_ERROR_ERROR_EN_INSTRUCCION:
                log_error(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Error Al Ejecutar Una Instrucción" COLOR_VERDE);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
                
            case QUERY_RESPONSE_ERROR_LECTURA_INVALIDA:
                log_error(qc->logger, COLOR_VERDE"## Query Finalizada - Error: Lectura Inválida" COLOR_VERDE);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
                
            case QUERY_RESPONSE_ERROR_QUERY_DESCONECTADO:
                log_error(qc->logger, COLOR_VERDE"## Query Finalizada - Error: Query Desconectado" COLOR_VERDE);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
                
            case QUERY_RESPONSE_ERROR_WORKER_DESCONECTADO:
                log_error(qc->logger, COLOR_VERDE "## Query Finalizada - Error: Worker Desconectado" COLOR_VERDE);
                list_destroy_and_destroy_elements(paqueteMaster, free);
                return;
                
            default:
                log_warning(qc->logger, "Código de operación desconocido: %d", codigo_operacion);
                break;
        }
        
        // Liberar el paquete después de procesarlo (excepto en los casos que ya retornaron)
        list_destroy_and_destroy_elements(paqueteMaster, free);
    }
}
