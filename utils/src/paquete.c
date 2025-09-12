#include "paquete.h"
#include "protocolo.h"
#include <string.h>   // Para memcpy
#include <sys/socket.h> // Para send

//Buffer
t_buffer *crear_buffer()
{
    t_buffer *buffer = malloc(sizeof(t_buffer));

    buffer->size = 0; 
    buffer->stream = NULL;

    return buffer;
}

//Paquete
t_paquete *crear_paquete(op_code code, t_buffer *buffer)
{
    t_paquete *packet = malloc(sizeof(t_paquete));

    packet->codigo_operacion = code;
    packet->buffer = buffer;

    return packet;
}

//Aniadir al paquete
void agregar_a_paquete(t_paquete *packet, void *stream, int size)
{
    packet->buffer->stream = realloc(packet->buffer->stream, packet->buffer->size + size + sizeof(int));

    memcpy(packet->buffer->stream + packet->buffer->size, &size, sizeof(int));
    memcpy(packet->buffer->stream + packet->buffer->size + sizeof(int), stream, size);

    packet->buffer->size += size + sizeof(int);
}

//Serializar paquete
void* serializar_paquete(t_paquete* paquete, int bytes, t_log* logger)
{
	if (!paquete || !paquete->buffer) {
        log_error(logger, "Intento de serializar paquete nulo");
        return NULL;
    }

	void * magic = malloc(bytes);
	if (!magic) {
        log_error(logger, "Malloc falló en serializar_paquete");
        return NULL;
    }

	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

//Enviar paquete
void enviar_paquete(t_paquete* paquete, int socket_cliente, t_log* logger)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes, logger);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

//eliminarPaquete
void eliminar_paquete(t_paquete *packet)
{
	if(packet != NULL)
	{
    	free(packet->buffer->stream);
    	free(packet->buffer);
    	free(packet);
	}
}

//Paquete del lado del SERVIDOR

t_list* recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}


void* recibir_buffer(int* size, int socket_cliente) 
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}