#ifndef OPERACIONES_H
#define OPERACIONES_H

// === Operaciones expuestas al Worker ===
bool crear_file(t_storage* storage, t_list* parametros);
bool truncar_file(t_storage* storage, t_list* parametros);
bool tag_file(t_storage* storage, t_list* parametros);
bool leer_bloque(t_storage* storage, t_list* parametros, void** contenido, int* tamanio_bloque);
bool escribir_bloque(t_storage* storage, t_list* parametros);
bool eliminar_file_tag(t_storage* storage, int query_id, const char* file, const char* tag);
bool realizar_commit(t_storage* storage, t_list* parametros);


// === Funciones auxiliares internas ===
void marcar_bloque_libre(t_storage* storage, int query_id, int numero_bloque);
int get_array_length(char** array);
char* path_logico_para_truncate(const char* punto_montaje, const char* nombre_file,
                                const char* tag, int i);
char* path_fisico_para_truncate(const char* punto_montaje, int bloque_fisico_id);
int* leer_bloques_actuales(t_config* metadata_config, int* cantidad_bloques_fisico);
char* serializar_bloques(const int* bloques, int cantidad_bloques);
char* calcular_md5_por_bloque(const char* path_bloque, int tamanio_bloque);
void evitar_duplicidad(t_storage* storage, char* file, char* tag);
void persistir_bitmap(t_storage* storage);
bool verificar_si_commited(t_storage* storage, const char* file, const char* tag);
char* serializar_bloques_array(char** bloques);

#endif 