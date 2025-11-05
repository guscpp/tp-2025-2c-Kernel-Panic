#ifndef STORAGE_H
#define STORAGE_H

#include "../../utils/include/utils.h"
#include <commons/string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <commons/bitarray.h>
#include <errno.h>

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
} t_storage;

typedef struct{
    int socket_cliente;
    t_storage* storage;
} args_hilo_worker;


t_storage* iniciar_storage();
bool inicializar_file_system(t_storage* storage);
void verificar_storage(t_storage* s);
void liberar_storage (t_storage* storage);
void enviar_tamanio_paquete_aworker(t_storage* storage, int worker_fd);

void rutina_recepcion(t_storage* storage, int storage_fd);
void* rutina_operaciones(void* args);

char* obtener_ruta_absoluta(char* ruta_rel);
void crear_directorios(char* ruta_rel);
void mkdir_recursivo(const char* ruta_abs);

#endif