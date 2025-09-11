#ifndef CONEXION_H_
#define CONEXION_H_

#include <sys/socket.h>
#include <commons/log.h>

// TODO
// Funciones de servidor
int iniciar_servidor(const char* ip, int puerto);
int esperar_cliente(int socket_servidor);

// TODO
// Función genérica para liberar una conexión
void liberar_conexion(int socket_cliente);

//no es preferible no enlazar conexion y logger dentro de una misma unidad logica?
//int crear_conexion(const char* ip, int puerto);
int crear_conexion(t_log *logger, char *ip, char *port);

#endif