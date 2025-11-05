#ifndef OPERACIONES_H
#define OPERACIONES_H

bool crear_file(t_storage* storage, t_list* parametros);
bool leer_bloque(t_storage* storage, t_list* parametros, void** contenido, int* tamanio_bloque);

#endif 