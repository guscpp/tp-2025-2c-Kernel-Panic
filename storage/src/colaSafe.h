#ifndef COLA_SAFE_H
#define COLA_SAFE_H

#include"servidorMultihilo.h"

cola_tareas* crear_cola_tareas();
void encolar_tarea(cola_tareas* cola, Tarea* tarea);
void* desencolar_tarea(cola_tareas* cola);
void destruir_cola_tareas(cola_tareas* cola);


#endif