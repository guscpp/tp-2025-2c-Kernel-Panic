#ifndef WORKER_H_
#define WORKER_H_

#include "../../utils/include/utils.h"
//#include "memoria.h"  
//#include "query_interpreter.h"
#include "tipos.h"
#include <semaphore.h>


t_worker* inicializar_worker(int id_worker);

void      verificar_worker(t_worker* worker);
void      liberar_worker(t_worker* w);

//void      recibir_path_de_query(int master_socket, t_log* logger);
Pcb*      recibir_path_de_query(int master_socket, t_worker* w);
void      rtas_storage(int storage_socket, t_worker* w);

FILE*     retornar_archivo(char* nombre_archivo, char* path_general, t_log* logger);

void*     ejecutar_query(void* arg);

void*     hilo_atender_interrupcion(void* arg);

bool      recibir_interrupciones(int master_socket, t_worker* w);

void retener_worker(t_worker* w);

void loggerError(t_log* logger, op_code etiqueta);
void flush_paginas_modificadas( t_memoria_interna* mem, int query_id, char* file, char* tag, int socket_storage);
void error_path_not_found(t_log* logger, op_code etiqueta, int id_query, char* path);
void error_tamanio_escrLectura_excedido(t_log* logger, op_code etiqueta, int id_query, char* file, char* tag);
void error_instruccion_malformada(t_log* logger,int id_query, char* instruccion);
void semaforos (t_worker* w);
#endif /* CLIENTE_H_*/