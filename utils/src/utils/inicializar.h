#ifndef UTILS_LOGGER_H_
#define UTILS_LOGGER_H_

#include <stdlib.h>
#include <stdio.h>

#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <sys/socket.h>
#include <netdb.h>



//Para los logs
t_log *iniciar_logger(char *logger_name, char *process_name, bool visible, t_log_level log_level);

t_config *iniciar_config(t_log *logger, char *path);

void terminar_programa(t_log *logger, t_config *config);

//Para clients
int crear_coneccion(t_log *logger, char *ip, char *port);



//Las "etiquetas"
typedef enum
{
	HANDSHAKE_CPU //Desde la cpu
}op_code;

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

#endif
