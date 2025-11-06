#ifndef OPERACIONES_H
#define OPERACIONES_H

// === Operaciones expuestas al Worker ===
bool crear_file(t_storage* storage, t_list* parametros);
bool truncar_file(t_storage* storage, t_list* parametros);
bool leer_bloque(t_storage* storage, t_list* parametros, void** contenido, int* tamanio_bloque);
bool eliminar_file_tag(t_storage* storage, int query_id, const char* file, const char* tag);

// === Funciones auxiliares internas ===
void marcar_bloque_libre(t_storage* storage, int numero_bloque);

#endif 