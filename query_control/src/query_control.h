// query_control/src/query_control.h
#ifndef QUERY_CONTROL_H_
#define QUERY_CONTROL_H_

#define COLOR_VERDE "\033[32m"

#include "../../utils/include/utils.h"
#include <unistd.h>      //Para close()

typedef struct {
    t_log*    logger;         /**< Logger para registrar eventos y mensajes del sistema */
    t_config* config;         /**< Configuración del módulo de consultas */
    char*     ip_master;      /**< Dirección IP del servidor master para conexión */
    int       puerto_master;  /**< Puerto del servidor master para conexión */
    char*     log_level;      /**< Nivel de log */
    char*     archivo_query;  /**< Ruta al archivo que contiene la consulta a ejecutar */
    int       prioridad;      /**< Nivel de prioridad de la consulta (menor = más prioritario) */
    int       master_socket;  /**< Socket de la conexion al servidor master*/
} t_query_control;

/**
 * @brief Inicializa la estructura de control de queries
 * 
 * @param argc Cantidad de argumentos de línea de comandos
 * @param argv Array de argumentos de línea de comandos
 * @return t_query_control* Puntero a la estructura inicializada, NULL en caso de error
 * 
 * @note Esta función se encarga de:
 * - Parsear los argumentos de entrada
 * - Cargar la configuración desde archivo
 * - Inicializar el logger
 * - Validar parámetros obligatorios
 */
t_query_control* inicializar_query_control(int argc, char* argv[]);

/**
 * @brief Verifica y muestra la configuración del Query Control
 * 
 * @param qc Puntero a la estructura de control de queries
 * 
 * @note Esta función loggea:
 * - si el Query Control se inicializó correctamente,
 * - la IP del servidor master configurada,
 * - el puerto de conexión al master,
 * - la ruta del archivo de query a procesar,
 * - el nivel de prioridad de la query,
 * - el nivel de logging configurado.
 * 
 * @warning Esta función asume que todos los campos de la estructura están
 *          correctamente inicializados. No realiza validaciones de valores.
 */
void verificar_query_control(t_query_control* qc);

/**
 * @brief Libera los recursos asociados a la estructura de control
 * 
 * @param qc Puntero a la estructura t_query_control a liberar
 * 
 * @note Libera memoria dinámica, cierra archivos y sockets abiertos
 */
void liberar_query_control(t_query_control* qc);

/**
 * @brief Establece conexión con el servidor master
 * 
 * @param qc Puntero a la estructura de control de queries
 * @return int Descriptor del socket conectado, -1 en caso de error
 * 
 * @note Utiliza los parámetros ip_master y puerto_master de la configuración
 */
int conectar_al_master(t_query_control* qc);

/**
 * @brief Procesa las respuestas recibidas desde el master
 * 
 * @param qc Puntero a la estructura de control de queries
 * 
 * @note Escucha continuamente las respuestas del master hasta que la conexión se cierre
 * u ocurra un error.
*/

void enviar_handshake (t_query_control* qc);
void enviar_path_y_prioridad(t_query_control *qc);
void procesar_respuestas_master(t_query_control* qc);

#endif /* QUERY_CONTROL_H_ */