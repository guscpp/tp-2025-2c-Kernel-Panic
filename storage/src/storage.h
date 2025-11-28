#ifndef STORAGE_H
#define STORAGE_H

#define _GNU_SOURCE // agregue esto para usar DT_REG
#include "../../utils/include/utils.h"
#include <commons/string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <commons/bitarray.h>
#include <errno.h>
#include <commons/crypto.h>
#include <dirent.h> // agregue esto para usar funciones sobre directorios

typedef struct {
    t_log* logger;
    t_config* config;          // storage.config
    t_config* superblock;      // superblock.config
    char* puerto_escucha;
    int fresh_start;
    char* punto_montaje;
    int retardo_operacion;
    int retardo_acceso_bloque;
    char* log_level;   
    int tamanio_bloque;
    int tamanio_filesystem;
    t_bitarray* bitmap;
    char* path_bitmap;
    int cantidad_workers;
    //int id_worker;    //en desuso, ahora es parte del t_worker_context

    pthread_mutex_t mutex_bitmap;       //protege al bitmap
    pthread_mutex_t mutex_hash_index;   //protege al archivo blocks_hash_index.config
    pthread_mutex_t mutex_workers;      //contexto de workers conectados al storage
    t_dictionary* dict_locks_files;     //clave: "file:tag", valor: pthread_mutex_t*
    pthread_mutex_t mutex_dict_locks;   //protege el diccionario en sí

} t_storage;

typedef struct{
    int socket_cliente;
    t_storage* storage;
    int worker_id;          //id de este worker
} t_worker_context;


// === Funciones de inicialización ===
t_storage* iniciar_storage(void);
bool inicializar_file_system(t_storage* storage);
void verificar_storage(t_storage* storage);
void destruir_storage(t_storage* storage);

// === Funciones de servidor ===
void  rutina_recepcion(t_storage* storage, int storage_fd);
void* rutina_operaciones(void* args);
void  enviar_tamanio_paquete_aworker(t_storage* storage, int worker_fd);

// === Utilidades FS ===
char* obtener_ruta_absoluta(char* ruta_rel);
void  crear_directorios(char* ruta_rel);
void  mkdir_recursivo(const char* ruta_abs);
int   rm_rf(const char* path);

// === Inicialización interna (usadas en storage.c) ===
void formatear_fs(t_storage* storage);
void crear_initial_file(t_storage* storage);
void recrear_bmap(t_storage* storage, int cantidad_bloques, char* path_bmap);
void recrear_hash(char* path_hash);

// === Utilidades ===
void destruir_mutex(char* key, void* value);
void destruir_mutex_lock(void* data);
void destruir_dict_locks(t_dictionary* dict);

// === Pruebas ===
void rutina_recepcion(t_storage* storage, int storage_fd);
bool crear_file(t_storage* storage, t_list* parametros);
bool truncar_file(t_storage* storage, t_list* parametros);
bool escribir_bloque(t_storage* storage, t_list* parametros);
bool escribir_bloque_test(t_storage* storage, char* file, char* tag,int bloque_logico, char* contenido);
bool realizar_commit(t_storage* storage, t_list* parametros);
bool eliminar_file_tag(t_storage* storage, t_list* parametros);

#endif