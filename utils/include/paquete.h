#ifndef PAQUETE_H_
#define PAQUETE_H_

#include <stdlib.h>
//#include <stdio.h>

#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <readline/readline.h>
//#include <sys/socket.h>
//#include <netdb.h>
#include "protocolo.h"


//Para el Buffer
typedef struct
{
	int size;
	void* stream;
} t_buffer;

t_buffer *crear_buffer();

//Para el Packet
typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

t_paquete *crear_paquete(op_code code, t_buffer *buffer);

void add_to_packet(t_paquete *packet, void *stream, int size);

void enviar_paquete(t_paquete *packet, int client_socket);

void eliminarPaquete(t_paquete *packet);


//Paquete del lado del SERVIDOR 

t_list* recibir_paquete(int);

void recibir_mensaje(int);

void* recibir_buffer(int* size, int socket_cliente) ;


#endif