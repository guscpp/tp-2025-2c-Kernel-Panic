#ifndef SERVIDOR_MULTIHILO_H
#define SERVIDOR_MULTIHILO_H

#include "storage.h"
#include <pthread.h>
#include <commons/collections/list.h>
#include <unistd.h>
#include "operaciones.h"

void rutina_recepcion(t_storage* storage, int storage_fd);
void* rutina_operaciones(void* args);
extern pthread_mutex_t mutex_fs;
#endif