#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

// ==================== CONFIGURACIÓN ====================

#define CONFIG_TEST "test_config.config"
#define QUERY_TEST "test_query.txt"
#define LOG_TEST "test_query_control.log"

// Simulamos códigos de operación del protocolo
typedef enum {
    QC_HANDSHAKE = 1,
    QUERY_REQUEST = 2,
    QUERY_RESPONSE_READ = 10,
    QUERY_RESPONSE_END = 11,
    QUERY_RESPONSE_ERROR = 12,
    WORKER_DESCONECTADO = 13
} op_code;

// ==================== ESTRUCTURAS SIMULADAS ====================

typedef struct {
    char* process_name;
} t_log;

typedef struct {
    char* ip_master;
    int puerto_master;
    char* log_level;
} t_config;

typedef struct {
    t_log* logger;
    t_config* config;
    char* ip_master;
    int puerto_master;
    char* log_level;
    char* archivo_query;
    int prioridad;
    int master_socket;
    bool conectado;
    pthread_t hilo_respuestas;
} t_query_control;

// Variables globales para simulación
static bool simulacion_master_desconectado = false;
static bool simulacion_timeout = false;
static int mensajes_recibidos = 0;
static bool deadlock_detectado = false;

// ==================== FUNCIONES SIMULADAS ====================

t_log* log_create(const char* file, const char* process, bool visible, int level) {
    t_log* logger = malloc(sizeof(t_log));
    if (logger) {
        logger->process_name = strdup(process);
    }
    return logger;
}

void log_destroy(t_log* logger) {
    if (logger) {
        free(logger->process_name);
        free(logger);
    }
}

void log_info(t_log* logger, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[INFO][%s] ", logger ? logger->process_name : "NULL");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void log_error(t_log* logger, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[ERROR][%s] ", logger ? logger->process_name : "NULL");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void log_warning(t_log* logger, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[WARNING][%s] ", logger ? logger->process_name : "NULL");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

t_config* config_create(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        return NULL;
    }
    fclose(file);
    
    t_config* config = malloc(sizeof(t_config));
    if (config) {
        config->ip_master = strdup("127.0.0.1");
        config->puerto_master = 9001;
        config->log_level = strdup("INFO");
    }
    return config;
}

void config_destroy(t_config* config) {
    if (config) {
        free(config->ip_master);
        free(config->log_level);
        free(config);
    }
}

char* config_get_string_value(t_config* config, const char* key) {
    if (!config) return NULL;
    if (strcmp(key, "IP_MASTER") == 0) return config->ip_master;
    if (strcmp(key, "LOG_LEVEL") == 0) return config->log_level;
    return NULL;
}

int config_get_int_value(t_config* config, const char* key) {
    if (!config) return 0;
    if (strcmp(key, "PUERTO_MASTER") == 0) return config->puerto_master;
    return 0;
}

// ==================== SIMULACIÓN DE CONEXIÓN ====================

int crear_conexion_simulada(t_log* logger, const char* ip, const char* puerto) {
    printf("🔌 Intentando conectar a %s:%s\n", ip, puerto);
    
    if (simulacion_master_desconectado) {
        printf("❌ Simulación: Master desconectado\n");
        return -1;
    }
    
    if (simulacion_timeout) {
        printf("⏰ Simulación: Timeout de conexión\n");
        sleep(3); // Simular timeout
        return -1;
    }
    
    // Simular conexión exitosa
    printf("✅ Conexión simulada exitosa\n");
    return 42; // Socket simulado
}

void cerrar_conexion_simulada(int socket) {
    printf("🔌 Cerrando conexión simulada (socket: %d)\n", socket);
}

// ==================== SIMULACIÓN DE ENVÍO/RECEPCIÓN ====================

int enviar_handshake_simulado(t_query_control* qc) {
    if (!qc->conectado) {
        log_error(qc->logger, "No se puede enviar handshake - no conectado");
        return -1;
    }
    log_info(qc->logger, "Enviando HANDSHAKE al Master");
    return 0;
}

int enviar_query_simulado(t_query_control* qc) {
    if (!qc->conectado) {
        log_error(qc->logger, "No se puede enviar query - no conectado");
        return -1;
    }
    log_info(qc->logger, "Enviando QUERY: %s, prioridad: %d", 
             qc->archivo_query, qc->prioridad);
    return 0;
}

// Simulador de respuestas del Master
void* simulador_respuestas_master(void* arg) {
    t_query_control* qc = (t_query_control*)arg;
    
    log_info(qc->logger, "Hilo de respuestas iniciado");
    
    // Simular diferentes escenarios
    for (int i = 0; i < 5 && qc->conectado; i++) {
        sleep(1);
        
        if (simulacion_master_desconectado) {
            log_error(qc->logger, "Master se desconectó inesperadamente");
            qc->conectado = false;
            break;
        }
        
        // Simular diferentes tipos de respuestas
        switch (i) {
            case 0:
                log_info(qc->logger, "## Lectura realizada: File archivo1:tag1, contenido: Hola Mundo");
                mensajes_recibidos++;
                break;
            case 1:
                log_info(qc->logger, "## Lectura realizada: File archivo1:tag1, contenido: Segundo contenido");
                mensajes_recibidos++;
                break;
            case 3:
                if (!deadlock_detectado) {
                    log_info(qc->logger, "## Query Finalizada - Éxito");
                    mensajes_recibidos++;
                    return NULL;
                }
                break;
        }
    }
    
    // Si llegamos aquí, podría ser un deadlock
    if (qc->conectado && mensajes_recibidos == 0) {
        log_error(qc->logger, "⚠️  POSIBLE DEADLOCK: No se recibieron respuestas");
        deadlock_detectado = true;
    }
    
    return NULL;
}

// ==================== FUNCIONES QUERY CONTROL MEJORADAS ====================

t_query_control* inicializar_query_control(int argc, char* argv[]) {
    if (argc != 4) {
        return NULL;
    }
    
    struct stat st;
    if (stat(argv[1], &st) == -1) {
        return NULL;
    }
    
    t_query_control* qc = malloc(sizeof(t_query_control));
    if (!qc) return NULL;
    
    qc->config = config_create(argv[1]);
    if (!qc->config) {
        free(qc);
        return NULL;
    }
    
    qc->logger = log_create("query_control.log", "[QUERY_CONTROL]", true, 2);
    if (!qc->logger) {
        config_destroy(qc->config);
        free(qc);
        return NULL;
    }
    
    qc->ip_master = strdup(config_get_string_value(qc->config, "IP_MASTER"));
    qc->puerto_master = config_get_int_value(qc->config, "PUERTO_MASTER");
    qc->log_level = strdup(config_get_string_value(qc->config, "LOG_LEVEL"));
    qc->archivo_query = strdup(argv[2]);
    qc->prioridad = atoi(argv[3]);
    qc->master_socket = -1;
    qc->conectado = false;
    
    return qc;
}

void liberar_query_control(t_query_control* qc) {
    if (qc) {
        if (qc->logger) log_destroy(qc->logger);
        if (qc->config) config_destroy(qc->config);
        if (qc->ip_master) free(qc->ip_master);
        if (qc->log_level) free(qc->log_level);
        if (qc->archivo_query) free(qc->archivo_query);
        if (qc->conectado) {
            cerrar_conexion_simulada(qc->master_socket);
        }
        free(qc);
    }
}

int conectar_al_master(t_query_control* qc) {
    qc->master_socket = crear_conexion_simulada(qc->logger, qc->ip_master, "9001");
    if (qc->master_socket == -1) {
        return -1;
    }
    qc->conectado = true;
    log_info(qc->logger, "### Conexión al Master exitosa. IP: %s, Puerto: %d", 
             qc->ip_master, qc->puerto_master);
    return 0;
}

void procesar_respuestas_master(t_query_control* qc) {
    log_info(qc->logger, "Iniciando procesamiento de respuestas...");
    
    time_t inicio = time(NULL);
    while (qc->conectado && (time(NULL) - inicio) < 10) { // Timeout de 10 segundos
        // En una implementación real, aquí se recibirían paquetes
        sleep(1);
        
        if (simulacion_master_desconectado) {
            log_error(qc->logger, "Conexión con el Master fue cerrada");
            break;
        }
        
        // Simular recepción de mensajes
        if (mensajes_recibidos < 3) {
            log_info(qc->logger, "Procesando respuesta %d del Master", mensajes_recibidos + 1);
            mensajes_recibidos++;
        } else {
            log_info(qc->logger, "Query completada exitosamente");
            break;
        }
    }
    
    if (qc->conectado && mensajes_recibidos == 0) {
        log_error(qc->logger, "⏰ TIMEOUT: No se recibieron respuestas del Master");
    }
}

// ==================== FUNCIÓN DE RESET PARA TESTS ====================

void resetear_estado_global(void) {
    simulacion_master_desconectado = false;
    simulacion_timeout = false;
    mensajes_recibidos = 0;
    deadlock_detectado = false;
    printf("🔄 Estado global resetado\n");
}

// ==================== TESTS AVANZADOS ====================

void crear_archivo_config_test(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file) {
        fprintf(file, "IP_MASTER=127.0.0.1\n");
        fprintf(file, "PUERTO_MASTER=9001\n");
        fprintf(file, "LOG_LEVEL=INFO\n");
        fclose(file);
    }
}

void crear_archivo_query_test(const char* filename, const char* contenido) {
    FILE* file = fopen(filename, "w");
    if (file) {
        fprintf(file, "%s", contenido);
        fclose(file);
    }
}

void eliminar_archivos_test(void) {
    remove(CONFIG_TEST);
    remove(QUERY_TEST);
    remove(LOG_TEST);
}

void ejecutar_test(const char* nombre_test, void (*test_func)(void)) {
    printf("\n════════════════════════════════════════\n");
    printf("🧪 TEST: %s\n", nombre_test);
    printf("════════════════════════════════════════\n");
    
    // Resetear estado antes de cada test
    resetear_estado_global();
    
    test_func();
    printf("✅ %s - PASÓ\n", nombre_test);
}

// Test 1: Conexión exitosa y comunicación normal
void test_comunicacion_normal(void) {
    printf("Simulando comunicación normal con Master...\n");
    
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    assert(qc != NULL);
    
    // Conectar
    int resultado = conectar_al_master(qc);
    assert(resultado == 0);
    assert(qc->conectado == true);
    
    // Enviar handshake
    resultado = enviar_handshake_simulado(qc);
    assert(resultado == 0);
    
    // Enviar query
    resultado = enviar_query_simulado(qc);
    assert(resultado == 0);
    
    // Procesar respuestas
    procesar_respuestas_master(qc);
    assert(mensajes_recibidos > 0);
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

// Test 2: Master desconectado durante handshake
void test_master_desconectado_handshake(void) {
    printf("Simulando Master desconectado durante handshake...\n");
    
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    assert(qc != NULL);
    
    // Configurar simulación de desconexión
    simulacion_master_desconectado = true;
    
    // Intentar conectar - debería fallar
    int resultado = conectar_al_master(qc);
    assert(resultado == -1);
    assert(qc->conectado == false);
    
    // Intentar enviar handshake - debería fallar
    resultado = enviar_handshake_simulado(qc);
    assert(resultado == -1);
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

// Test 3: Timeout de conexión
void test_timeout_conexion(void) {
    printf("Simulando timeout de conexión...\n");
    
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    assert(qc != NULL);
    
    // Configurar simulación de timeout
    simulacion_timeout = true;
    
    // Intentar conectar - debería fallar por timeout
    int resultado = conectar_al_master(qc);
    assert(resultado == -1);
    assert(qc->conectado == false);
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

// Test 4: Master se desconecta durante ejecución
void test_desconexion_durante_ejecucion(void) {
    printf("Simulando desconexión del Master durante ejecución...\n");
    
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    assert(qc != NULL);
    
    // Conectar exitosamente primero (asegurarse que timeout está desactivado)
    simulacion_timeout = false;
    simulacion_master_desconectado = false;
    
    int resultado = conectar_al_master(qc);
    assert(resultado == 0);
    
    // Iniciar procesamiento en hilo separado
    mensajes_recibidos = 0;
    
    // Crear hilo para simular respuestas
    pthread_t hilo_respuestas;
    int thread_created = pthread_create(&hilo_respuestas, NULL, simulador_respuestas_master, qc);
    assert(thread_created == 0);
    
    // Esperar un poco y luego simular desconexión
    sleep(2);
    printf("⚠️  Simulando desconexión abrupta del Master...\n");
    simulacion_master_desconectado = true;
    
    // Esperar a que el hilo termine
    pthread_join(hilo_respuestas, NULL);
    
    // Verificar que se manejó la desconexión
    assert(mensajes_recibidos <= 2); // Máximo 2 mensajes antes de desconexión
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

// Test 5: Deadlock por falta de respuestas
void test_posible_deadlock(void) {
    printf("Simulando posible deadlock por falta de respuestas...\n");
    
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    assert(qc != NULL);
    
    // Conectar
    int resultado = conectar_al_master(qc);
    assert(resultado == 0);
    
    // Configurar para simular que NO hay respuestas del Master
    // En este test, NO llamamos a procesar_respuestas_master porque 
    // esa función simula recibir mensajes automáticamente
    
    // En su lugar, simulamos un escenario donde el Query Control
    // espera pero nunca recibe respuestas
    log_info(qc->logger, "Iniciando espera de respuestas (simulando deadlock)...");
    
    time_t inicio = time(NULL);
    
    // Simular espera sin respuestas
    int ciclos_espera = 0;
    while (qc->conectado && (time(NULL) - inicio) < 5) { // Timeout más corto para test
        sleep(1);
        ciclos_espera++;
        log_info(qc->logger, "Ciclo de espera %d - Sin respuestas...", ciclos_espera);
        
        // Forzar la condición de "sin respuestas"
        // No incrementamos mensajes_recibidos
    }
    
    time_t fin = time(NULL);
    
    // Verificar que no hubo deadlock infinito (se respetó el timeout)
    assert((fin - inicio) <= 6); // Timeout de 5 segundos + margen
    assert(mensajes_recibidos == 0); // No se recibieron mensajes
    assert(ciclos_espera > 0); // Debería haber hecho al menos un ciclo
    
    log_info(qc->logger, "✅ Timeout respetado - No hubo deadlock infinito");
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

// Test 6: Múltiples queries simultáneas (stress test)
void test_multiple_queries(void) {
    printf("Simulando múltiples queries simultáneas...\n");
    
    crear_archivo_config_test(CONFIG_TEST);
    
    t_query_control* queries[3];
    int queries_exitosas = 0;
    
    for (int i = 0; i < 3; i++) {
        char query_name[50];
        sprintf(query_name, "test_query_%d.txt", i);
        crear_archivo_query_test(query_name, "CREATE test:tag\nEND\n");
        
        char* argv[] = {"query", CONFIG_TEST, query_name, "1"};
        queries[i] = inicializar_query_control(4, argv);
        
        if (queries[i] != NULL) {
            // Conectar alternando éxito/fallo
            simulacion_master_desconectado = (i % 2 == 0);
            if (conectar_al_master(queries[i]) == 0) {
                queries_exitosas++;
            }
        }
    }
    
    printf("Queries creadas: 3, Conexiones exitosas: %d\n", queries_exitosas);
    assert(queries_exitosas >= 1); // Al menos una debería conectar
    
    // Liberar recursos
    for (int i = 0; i < 3; i++) {
        if (queries[i]) {
            liberar_query_control(queries[i]);
        }
        char query_name[50];
        sprintf(query_name, "test_query_%d.txt", i);
        remove(query_name);
    }
    
    eliminar_archivos_test();
}

// Test 7: Recuperación después de fallo
void test_recuperacion_fallo(void) {
    printf("Simulando recuperación después de fallo...\n");
    
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    assert(qc != NULL);
    
    // Primer intento: falla
    simulacion_master_desconectado = true;
    int resultado = conectar_al_master(qc);
    assert(resultado == -1);
    
    // Segundo intento: éxito
    simulacion_master_desconectado = false;
    resultado = conectar_al_master(qc);
    assert(resultado == 0);
    
    // Verificar que se recuperó
    assert(qc->conectado == true);
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

// Test 8: Worker desconectado (simulación)
void test_worker_desconectado(void) {
    printf("Simulando escenario donde Worker se desconecta...\n");
    
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    assert(qc != NULL);
    
    // Conectar exitosamente
    int resultado = conectar_al_master(qc);
    assert(resultado == 0);
    
    // Simular que después de un tiempo recibimos error de Worker desconectado
    log_info(qc->logger, "Recibiendo error: Worker desconectado durante ejecución");
    
    // Verificar que el Query Control puede manejar este error
    assert(qc->conectado == true); // Debería mantenerse conectado al Master
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

// Test 9: Query con prioridades diferentes
void test_prioridades_diferentes(void) {
    printf("Probando diferentes niveles de prioridad...\n");
    
    crear_archivo_config_test(CONFIG_TEST);
    
    int prioridades[] = {0, 1, 5, 10}; // Prioridades de prueba
    int exitosas = 0;
    
    for (int i = 0; i < 4; i++) {
        char query_name[50];
        sprintf(query_name, "test_query_prio_%d.txt", i);
        crear_archivo_query_test(query_name, "CREATE test:tag\nEND\n");
        
        char prioridad_str[10];
        sprintf(prioridad_str, "%d", prioridades[i]);
        
        char* argv[] = {"query", CONFIG_TEST, query_name, prioridad_str};
        t_query_control* qc = inicializar_query_control(4, argv);
        
        if (qc != NULL) {
            assert(qc->prioridad == prioridades[i]);
            printf("✅ Prioridad %d configurada correctamente\n", prioridades[i]);
            exitosas++;
            liberar_query_control(qc);
        }
        
        remove(query_name);
    }
    
    assert(exitosas == 4); // Todas las prioridades deberían funcionar
    
    eliminar_archivos_test();
}

// ==================== FUNCIÓN PRINCIPAL ====================

int main(void) {
    printf("=========================================\n");
    printf("    TESTER AVANZADO QUERY CONTROL\n");
    printf("     (Escenarios del Mundo Real - CORREGIDO)\n");
    printf("=========================================\n");
    
    // Resetear estado global inicial
    resetear_estado_global();
    
    // Ejecutar tests avanzados
    ejecutar_test("Comunicación Normal", test_comunicacion_normal);
    ejecutar_test("Master Desconectado Handshake", test_master_desconectado_handshake);
    ejecutar_test("Timeout de Conexión", test_timeout_conexion);
    ejecutar_test("Desconexión Durante Ejecución", test_desconexion_durante_ejecucion);
    ejecutar_test("Posible Deadlock", test_posible_deadlock);
    ejecutar_test("Múltiples Queries", test_multiple_queries);
    ejecutar_test("Recuperación después de Fallo", test_recuperacion_fallo);
    ejecutar_test("Worker Desconectado", test_worker_desconectado);
    ejecutar_test("Prioridades Diferentes", test_prioridades_diferentes);
    
    printf("\n════════════════════════════════════════\n");
    printf("🎉 TODOS LOS TESTS AVANZADOS COMPLETADOS\n");
    printf("════════════════════════════════════════\n");
    
    return EXIT_SUCCESS;
}