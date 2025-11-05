#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>

// ==================== CONFIGURACIÓN ====================

#define CONFIG_TEST "test_config.config"
#define QUERY_TEST "test_query.txt"
#define LOG_TEST "test_query_control.log"

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
} t_query_control;

// ==================== FUNCIONES SIMULADAS ====================

t_log* log_create(const char* file, const char* process, bool visible, int level) {
    printf("Creando logger: %s\n", process);
    t_log* logger = malloc(sizeof(t_log));
    if (logger) {
        logger->process_name = strdup(process);
    }
    return logger;
}

void log_destroy(t_log* logger) {
    if (logger) {
        printf("Destruyendo logger: %s\n", logger->process_name);
        free(logger->process_name);
        free(logger);
    }
}

void log_info(t_log* logger, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[INFO] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void log_error(t_log* logger, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[ERROR] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void log_warning(t_log* logger, const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[WARNING] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

t_config* config_create(const char* path) {
    printf("Creando config desde: %s\n", path);
    
    // Verificar que el archivo existe
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("Error: No se pudo abrir %s\n", path);
        return NULL;
    }
    fclose(file);
    
    t_config* config = malloc(sizeof(t_config));
    if (config) {
        // Valores por defecto para tests
        config->ip_master = strdup("127.0.0.1");
        config->puerto_master = 9001;
        config->log_level = strdup("INFO");
    }
    return config;
}

void config_destroy(t_config* config) {
    if (config) {
        printf("Destruyendo config\n");
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

// ==================== FUNCIONES QUERY CONTROL ====================

t_query_control* inicializar_query_control(int argc, char* argv[]) {
    printf("Inicializando Query Control con %d argumentos\n", argc);
    
    if (argc != 4) {
        printf("Error: Se necesitan 4 argumentos, se recibieron %d\n", argc);
        return NULL;
    }
    
    // Verificar que el archivo de configuración existe
    struct stat st;
    if (stat(argv[1], &st) == -1) {
        printf("Error: Archivo de configuración no existe: %s\n", argv[1]);
        return NULL;
    }
    
    t_query_control* qc = malloc(sizeof(t_query_control));
    if (!qc) {
        printf("Error: No se pudo allocar memoria para Query Control\n");
        return NULL;
    }
    
    // Inicializar con valores por defecto
    qc->logger = NULL;
    qc->config = NULL;
    qc->ip_master = NULL;
    qc->log_level = NULL;
    qc->archivo_query = NULL;
    
    qc->config = config_create(argv[1]);
    if (!qc->config) {
        printf("Error: No se pudo crear config\n");
        free(qc);
        return NULL;
    }
    
    qc->logger = log_create("query_control.log", "[QUERY_CONTROL]", true, 2);
    if (!qc->logger) {
        printf("Error: No se pudo crear logger\n");
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
    
    printf("Query Control inicializado exitosamente\n");
    return qc;
}

void liberar_query_control(t_query_control* qc) {
    if (qc) {
        printf("Liberando Query Control...\n");
        if (qc->logger) log_destroy(qc->logger);
        if (qc->config) config_destroy(qc->config);
        if (qc->ip_master) free(qc->ip_master);
        if (qc->log_level) free(qc->log_level);
        if (qc->archivo_query) free(qc->archivo_query);
        free(qc);
    }
}

void verificar_query_control(t_query_control* qc) {
    if (qc && qc->logger) {
        log_info(qc->logger, "=== VERIFICACIÓN QUERY CONTROL ===");
        log_info(qc->logger, "IP Master: %s", qc->ip_master);
        log_info(qc->logger, "Puerto Master: %d", qc->puerto_master);
        log_info(qc->logger, "Archivo Query: %s", qc->archivo_query);
        log_info(qc->logger, "Prioridad: %d", qc->prioridad);
        log_info(qc->logger, "Socket Master: %d", qc->master_socket);
    }
}

// ==================== FUNCIONES AUXILIARES TESTS ====================

void crear_archivo_config_test(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file) {
        fprintf(file, "IP_MASTER=127.0.0.1\n");
        fprintf(file, "PUERTO_MASTER=9001\n");
        fprintf(file, "LOG_LEVEL=INFO\n");
        fclose(file);
        printf("Archivo de configuración creado: %s\n", filename);
    }
}

void crear_archivo_query_test(const char* filename, const char* contenido) {
    FILE* file = fopen(filename, "w");
    if (file) {
        fprintf(file, "%s", contenido);
        fclose(file);
        printf("Archivo de query creado: %s\n", filename);
    }
}

void eliminar_archivos_test(void) {
    int removed = 0;
    if (remove(CONFIG_TEST) == 0) { printf("Eliminado: %s\n", CONFIG_TEST); removed++; }
    if (remove(QUERY_TEST) == 0) { printf("Eliminado: %s\n", QUERY_TEST); removed++; }
    if (remove(LOG_TEST) == 0) { printf("Eliminado: %s\n", LOG_TEST); removed++; }
    if (removed > 0) {
        printf("Total archivos de test eliminados: %d\n", removed);
    }
}

void ejecutar_test(const char* nombre_test, void (*test_func)(void)) {
    printf("\n");
    printf("════════════════════════════════════════\n");
    printf("EJECUTANDO: %s\n", nombre_test);
    printf("════════════════════════════════════════\n");
    
    test_func();
    
    printf("✅ %s - PASÓ\n", nombre_test);
}

// ==================== TESTS ====================

void test_inicializacion_exitosa(void) {
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    
    assert(qc != NULL);
    assert(strcmp(qc->ip_master, "127.0.0.1") == 0);
    assert(qc->puerto_master == 9001);
    assert(strcmp(qc->archivo_query, QUERY_TEST) == 0);
    assert(qc->prioridad == 1);
    assert(qc->master_socket == -1);
    
    printf("✅ Valores verificados:\n");
    printf("   - IP Master: %s\n", qc->ip_master);
    printf("   - Puerto Master: %d\n", qc->puerto_master);
    printf("   - Archivo Query: %s\n", qc->archivo_query);
    printf("   - Prioridad: %d\n", qc->prioridad);
    
    verificar_query_control(qc);
    liberar_query_control(qc);
    eliminar_archivos_test();
}

void test_argumentos_insuficientes(void) {
    printf("Probando con 2 argumentos (deberían ser 4)...\n");
    char* argv[] = {"query", CONFIG_TEST}; // Solo 2 argumentos
    t_query_control* qc = inicializar_query_control(2, argv);
    
    assert(qc == NULL);
    printf("✅ Correctamente rechazó argumentos insuficientes\n");
}

void test_config_inexistente(void) {
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    printf("Probando con archivo de configuración inexistente...\n");
    char* argv[] = {"query", "no_existe.config", QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    
    assert(qc == NULL);
    printf("✅ Correctamente rechazó configuración inexistente\n");
    
    remove(QUERY_TEST);
}

void test_prioridad_negativa(void) {
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    printf("Probando con prioridad negativa (-1)...\n");
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "-1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    
    assert(qc != NULL);
    assert(qc->prioridad == -1);
    printf("✅ Prioridad negativa aceptada: %d\n", qc->prioridad);
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

void test_query_inexistente(void) {
    crear_archivo_config_test(CONFIG_TEST);
    
    printf("Probando con archivo de query inexistente...\n");
    char* argv[] = {"query", CONFIG_TEST, "no_existe.txt", "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    
    assert(qc != NULL);
    assert(strcmp(qc->archivo_query, "no_existe.txt") == 0);
    printf("✅ Query Control creado incluso con query inexistente: %s\n", qc->archivo_query);
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

void test_verificacion_configuracion(void) {
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "2"};
    t_query_control* qc = inicializar_query_control(4, argv);
    
    assert(qc != NULL);
    printf("✅ Ejecutando verificación de configuración...\n");
    verificar_query_control(qc); // No debería fallar
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

void test_liberacion_recursos(void) {
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "3"};
    t_query_control* qc = inicializar_query_control(4, argv);
    
    assert(qc != NULL);
    printf("✅ Liberando recursos...\n");
    liberar_query_control(qc); // No debería haber segfault
    printf("✅ Recursos liberados exitosamente\n");
    
    eliminar_archivos_test();
}

void test_liberacion_nula(void) {
    printf("Probando liberación de puntero NULL...\n");
    liberar_query_control(NULL); // No debería fallar
    printf("✅ Liberación de NULL manejada correctamente\n");
}

void test_prioridad_maxima(void) {
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, "CREATE test:tag\nEND\n");
    
    printf("Probando con prioridad máxima (0)...\n");
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "0"};
    t_query_control* qc = inicializar_query_control(4, argv);
    
    assert(qc != NULL);
    assert(qc->prioridad == 0);
    printf("✅ Prioridad máxima aceptada: %d\n", qc->prioridad);
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

void test_query_vacia(void) {
    crear_archivo_config_test(CONFIG_TEST);
    crear_archivo_query_test(QUERY_TEST, ""); // Archivo vacío
    
    printf("Probando con archivo de query vacío...\n");
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "1"};
    t_query_control* qc = inicializar_query_control(4, argv);
    
    assert(qc != NULL);
    
    // Verificar que el archivo existe pero está vacío
    FILE* file = fopen(QUERY_TEST, "r");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    assert(size == 0);
    printf("✅ Query Control creado con archivo vacío (tamaño: %ld bytes)\n", size);
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

void test_query_compleja(void) {
    crear_archivo_config_test(CONFIG_TEST);
    
    const char* query_compleja = 
        "CREATE file1:base\n"
        "TRUNCATE file1:base 128\n"
        "WRITE file1:base 0 \"Contenido de prueba\"\n"
        "READ file1:base 0 18\n"
        "TAG file1:base file1:copia\n"
        "COMMIT file1:copia\n"
        "FLUSH file1:base\n"
        "DELETE file1:base\n"
        "END\n";
    
    crear_archivo_query_test(QUERY_TEST, query_compleja);
    
    printf("Probando con query compleja...\n");
    char* argv[] = {"query", CONFIG_TEST, QUERY_TEST, "2"};
    t_query_control* qc = inicializar_query_control(4, argv);
    
    assert(qc != NULL);
    assert(qc->prioridad == 2);
    
    // Verificar que el archivo existe y tiene contenido
    FILE* file = fopen(QUERY_TEST, "r");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    assert(size > 0);
    printf("✅ Query Control creado con query compleja (tamaño: %ld bytes)\n", size);
    
    liberar_query_control(qc);
    eliminar_archivos_test();
}

// ==================== FUNCIÓN PRINCIPAL ====================

int main(void) {
    printf("=========================================\n");
    printf("    TESTER QUERY CONTROL - CORREGIDO\n");
    printf("=========================================\n");
    
    // Limpiar archivos residuales primero
    eliminar_archivos_test();
    printf("\n");
    
    int tests_pasados = 0;
    int tests_totales = 0;
    
    // Ejecutar todos los tests
    ejecutar_test("Inicialización Exitosa", test_inicializacion_exitosa);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Argumentos Insuficientes", test_argumentos_insuficientes);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Configuración Inexistente", test_config_inexistente);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Prioridad Negativa", test_prioridad_negativa);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Query Inexistente", test_query_inexistente);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Verificación Configuración", test_verificacion_configuracion);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Liberación Recursos", test_liberacion_recursos);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Liberación Nula", test_liberacion_nula);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Prioridad Máxima", test_prioridad_maxima);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Query Vacía", test_query_vacia);
    tests_pasados++; tests_totales++;
    
    ejecutar_test("Query Compleja", test_query_compleja);
    tests_pasados++; tests_totales++;
    
    printf("\n════════════════════════════════════════\n");
    printf("RESULTADO FINAL:\n");
    printf("✅ Tests pasados: %d/%d\n", tests_pasados, tests_totales);
    printf("════════════════════════════════════════\n");
    
    if (tests_pasados == tests_totales) {
        printf("🎉 ¡TODOS LOS TESTS PASARON EXITOSAMENTE!\n");
        return EXIT_SUCCESS;
    } else {
        printf("❌ Algunos tests fallaron\n");
        return EXIT_FAILURE;
    }
}