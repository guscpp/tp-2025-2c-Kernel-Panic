#ifndef QUERY_INTERPRETER_H_
#define QUERY_INTERPRETER_H_

#include "../../utils/include/utils.h"
#include "tipos.h" //porque usa t_worker sus funciones

extern bool query_desconectado;

t_query_interpreter*    query_interpreter_crear(t_log* logger);
void      query_interpreter_ciclo(Pcb* pcb, t_worker* w);
char*     fetch(Pcb* pcb, t_worker* w);
t_decode* decode(char* instruccion, t_worker* w, Pcb* pcb);
void      execute(t_instr_param* parametros, void (*ejecuta_instruccion)(t_instr_param*, t_worker*, Pcb*), t_worker* w, Pcb* pcb);
char**    aux_separar_file_tag(char* cadena);


void executeCreate(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void executeTruncate(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void executeWrite(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void executeRead(t_instr_param* parametros, t_worker* w, Pcb* pcb); 
void executeTag(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void executeCommit(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void executeFlush(t_instr_param* parametros, t_worker* w, Pcb* pcb);
void executeDelete(t_instr_param* parametros, t_worker* w, Pcb* pcb);

//no forma parte del execute y entonces tampoco esta dentro del struct. Se hace aparte porque la identificas en el decode, no llega a execute
void executeEnd(t_worker* w, Pcb* pcb);

void interrupt_envio_a_master(Pcb* pcb_dsp_de_interrupt, t_worker* w);
void error_instruccion_malformada(t_log* logger,int id_query, char* instruccion);
void destruir_decode(t_decode* dec);

#endif