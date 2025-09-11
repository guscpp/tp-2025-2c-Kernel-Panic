#include <paquete.h>
#include <protocolo.h>
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
void add_to_packet(t_paquete *packet, void *stream, int size)
{
    packet->buffer->stream = realloc(packet->buffer->stream, packet->buffer->size + size + sizeof(int));

    memcpy(packet->buffer->stream + packet->buffer->size, &size, sizeof(int));
    memcpy(packet->buffer->stream + packet->buffer->size + sizeof(int), stream, size);

    packet->buffer->size += size + sizeof(int);
}

//Serializar paquete
void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
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
void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

//eliminarPaquete
void eliminarPaquete(t_paquete *packet)
{
    free(packet->buffer->stream);
    free(packet->buffer);
    free(packet);
}