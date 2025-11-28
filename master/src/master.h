#ifndef MASTER_H_
#define MASTER_H_

#include "../../utils/include/utils.h"
#include <unistd.h>
#include <semaphore.h>

// ============================================================================
#define COLOR_VERDE "\033[32m"

// ============================================================================
typedef struct {
    t_log* logger;
    int socket_conexion;
    int id;
} t_hacerConnect;

typedef struct {
    int id;                    // ID único asignado por el Master
    char* path;                // Ruta del archivo de la query
    int prioridad;             // Prioridad (menor número = mayor prioridad)
    int socket; 
    int programCounter;
    int idWorker;
    int estado;
    bool alive;                // Solo para queries
    pthread_t hilo_timer;      // Solo para queries
    t_log* logger;
    sem_t timer_query;
} t_query;

typedef struct {
    char* contenido;  
    char* file;   
    char* tag;  
} t_readQuery;

typedef struct {
    int id;
    int socket;
    int socket_interruption;
    bool desconection;         // Para manejar desconexiones
    t_log* logger;
    sem_t desconexion_worker;
    // int idQuery;
} t_worker;

// ============================================================================
enum estado {
    READY,
    RUNNING,
    EXIT
};

// ============================================================================
extern int tiempo_aging;
extern char* algoritmo_planificacion;

// ============================================================================
// Funciones de conexión y atención
void* atender_conexion(void*);
void  atender_Query(t_hacerConnect*);
void  atender_Worker(t_hacerConnect*);
void* atender_desconexion_query(void*);
void* atender_desconexion_worker(void*);

// Funciones de inicialización y configuración
void inicializar_semaforos(t_log*);

// Funciones de gestión de queries
void asignar_id_query(int*);
void comenzar_a_ejecutar(t_hacerConnect*, int);
void enviar_query_a_worker(t_query*, t_hacerConnect*, int);
void enviar_read_a_query(t_query*, t_readQuery*, t_hacerConnect*);
void query_completado_con_exito(t_query*, t_hacerConnect*);

// Funciones de búsqueda y manipulación de listas
t_query* eliminar_por_id(t_list*, int);
t_query* obtener_por_id(t_list*, int);
t_query* obtener_menor_prioridad(t_list*);
t_query* obtener_query_por_id_worker(t_list*, int);

// Funciones de workers
t_worker* obtener_por_id_worker(t_list*, int);
t_worker* eliminar_worker_por_id(t_list*, int);

// Funciones de planificación y desalojo
void  chequeador_desalojo(int prioridad, t_log*);
void* _max_prioridad(void*, void*);
void  atender_worker_interrupt(t_hacerConnect*, int);
void  realizar_desalojo(int, int, int, t_log*, int);
t_query* obtener_mayor_prioridad(t_list* lista_queries);

// Funciones de temporización
void* atender_timer_query(void*);

#endif /* MASTER_H_ */