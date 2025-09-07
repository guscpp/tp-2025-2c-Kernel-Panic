//#include <utils/logger.h>

#include "inicializar.h"

t_log *iniciar_logger(char *logger_name, char *process_name, bool visible, t_log_level log_level)
{
    t_log *new_logger;

    new_logger = log_create(logger_name, process_name, visible, log_level);

    if (new_logger == NULL)
    {
        printf("Error, no se pudo crear el logger\n");
        exit(1);
    }

    return new_logger;
}

t_config *iniciar_config(t_log *logger, char *path)
{
    t_config *new_config = NULL;

    new_config = config_create(path);

    if (new_config == NULL)
    {
        log_error(logger, "Error al crear el archivo de configuracion");
        exit(1);
    }

    log_info(logger, "Archivo de configuracion creado correctamente");

    return new_config;
}

void terminar_programa(t_log *logger, t_config *config)
{
    log_destroy(logger);
    config_destroy(config);
}

//-----------PARTE DE SOCKETS (se puede separar para mas prolijidad)

//Cliente
/*
int crear_conexion(t_log *logger, char *ip, char *port)
{
    int client_socket;
    struct addrinfo hints;
    struct addrinfo *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(ip, port, &hints, &server_info);

    // Create client socket
    client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

    if (client_socket == -1)
    {
        log_error(logger, "Error al crear el socket");
        exit(1);
    }

    // Connect socket
    int conection = connect(client_socket, server_info->ai_addr, server_info->ai_addrlen);

    if (conection == -1)
    {
        log_error(logger, "Error al conectar el socket");
        exit(1);
    }

    freeaddrinfo(server_info);

    return client_socket;
}
*/
//-----------------PROTOCOLO--------------------------------
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