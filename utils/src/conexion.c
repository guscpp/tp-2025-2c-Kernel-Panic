#include "conexion.h"
#include <commons/log.h>	// Para t_log
#include <commons/config.h> // Para t_config
#include <netdb.h>			// Para getaddrinfo, struct addrinfo
#include <string.h>			// Para memset
#include <stdlib.h>			// Para exit
#include <unistd.h>			// Para close
#include <errno.h>

// Conexion del lado del cliente
int crear_conexion(t_log *logger, char *ip, char *port)
{
	int client_socket;
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int result = getaddrinfo(ip, port, &hints, &server_info);
	if (result != 0)
	{
		log_error(logger, "getaddrinfo falló: %s", gai_strerror(result));
        return -1;
	}

	// Create client socket
	client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	if (client_socket == -1)
	{
        log_error(logger, "Error al crear socket: %s (utils/crear_conexion)", strerror(errno));
        freeaddrinfo(server_info);
        exit(1);
	}

	// Connect socket
	int conection = connect(client_socket, server_info->ai_addr, server_info->ai_addrlen);

	if (conection == -1)
	{
        log_error(logger, "Error al conectar: %s (utils/crear_conexion)", strerror(errno));
        close(client_socket);
        freeaddrinfo(server_info);
        exit(1);
	}

	freeaddrinfo(server_info);
	return client_socket;
}

// Conexion del lado del server
int iniciar_servidor(const char* puerto)
{
	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	int socket_servidor = socket(servinfo->ai_family,
								 servinfo->ai_socktype,
								 servinfo->ai_protocol);

	// permite que varios sockets se puedan bindear a un puerto al mismo tiempo,
	// siempre y cuando pertenezcan al mismo usuario
	// setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);
	// log_trace(logger, "Listo para escuchar a mi cliente");

	return socket_servidor;
}
int iniciar_servidor2(const char* puerto)
{
    struct addrinfo hints, *servinfo;
    int socket_servidor;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv;
    if ((rv = getaddrinfo(NULL, puerto, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(EXIT_FAILURE);
    }

    socket_servidor = socket(servinfo->ai_family,
                             servinfo->ai_socktype,
                             servinfo->ai_protocol);
    if (socket_servidor < 0) {
        perror("socket");
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    // Permite reusar el puerto aunque esté en TIME_WAIT
    if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
        perror("setsockopt");
        close(socket_servidor);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    if (bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("bind");
        close(socket_servidor);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    if (listen(socket_servidor, SOMAXCONN) < 0) {
        perror("listen");
        close(socket_servidor);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);

    return socket_servidor;
}


int esperar_cliente(int socket_servidor)
{
	// Aceptamos un nuevo cliente
	int socket_cliente = accept(socket_servidor, NULL, NULL);

	return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}
