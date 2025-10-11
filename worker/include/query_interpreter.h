#ifndef QUERY_INTERPRETER_H_
#define QUERY_INTERPRETER_H_

#include "worker.h"
#include "../../utils/include/utils.h"
//#include "memoria.h"
#include "tipos.h"

// Modelo para que haya algo, 
t_query_interpreter*    query_interpreter_crear(t_log* logger);

void query_interpreter_ciclo(Pcb* pcb, t_worker* w);
char* fetch(Pcb* pcb, t_worker* w);
t_decode* decode(char* instruccion, t_worker* w);
void execute(t_instr_param* parametros, void (*ejecuta_instruccion)(t_instr_param*, t_worker*), t_worker* w);
char** aux_separar_file_tag(char* cadena);
void executeCreate(t_instr_param* parametros, t_worker* w);
void executeTruncate(t_instr_param* parametros, t_worker* w);
void executeWrite(t_instr_param* parametros, t_worker* w);
void executeRead(t_instr_param* parametros, t_worker* w);
void executeTag(t_instr_param* parametros, t_worker* w);
void executeCommit(t_instr_param* parametros, t_worker* w);
void executeFlush(t_instr_param* parametros, t_worker* w);
void executeDelete(t_instr_param* parametros, t_worker* w);
void executeEnd(t_worker* w);
void interrupt_envio_a_master(Pcb* pcb_dsp_de_interrupt, t_worker* w);

#endif