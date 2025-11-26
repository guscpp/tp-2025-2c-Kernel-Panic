#ifndef WORKER_H_
#define WORKER_H_

#include "../../utils/include/utils.h"
#include "tipos.h" //por t_worker
#include <semaphore.h>
#include "memoria.h"

t_worker* inicializar_worker(int id_worker, char* config);
void      verificar_worker(t_worker* worker);
void      liberar_worker(t_worker* w);

//void      recibir_path_de_query(int master_socket, t_log* logger);
Pcb*      recibir_path_de_query(int master_socket, t_worker* w);
void      rtas_storage(int storage_socket, t_worker* w, t_instr_param* parametros, Pcb* pcb);
FILE*     retornar_archivo(char* nombre_archivo, char* path_general, t_log* logger);
void*     ejecutar_query(void* arg);
void*     hilo_atender_interrupcion(void* arg);
bool      recibir_interrupciones(int master_socket, t_worker* w);
void      retener_worker(t_worker* w);
void      loggerError(t_log* logger, op_code etiqueta);

//Errores
void error_path_not_found(t_log* logger, op_code etiqueta, int id_query, char* path);
void error_tamanio_escrLectura_excedido(t_log* logger, op_code etiqueta, int id_query, char* file, char* tag);
void error_instruccion_malformada(t_log* logger,int id_query, char* instruccion);
void informar_error_create(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void informar_error_truncate(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void informar_error_write(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void informar_error_read(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void informar_error_tag(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void informar_error_commit(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void informar_error_delete(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void informar_error_flush(t_instr_param* parametros, t_worker* w, Pcb* pcb);

//Semaforos
void semaforos (t_worker* w);

storage_error* inicializar_mutex_error_storage(t_worker* w);
#endif /* CLIENTE_H_*/