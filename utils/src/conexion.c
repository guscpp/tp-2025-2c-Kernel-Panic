#include <conexion.h>
#include <commons/log.h>     // Para t_log
#include <commons/config.h>  // Para t_config
#include <netdb.h>       // Para getaddrinfo, struct addrinfo
#include <string.h>      // Para memset
#include <stdlib.h>      // Para exit
#include <unistd.h>      // Para close

//no es preferible no enlazar conexion y logger dentro de una misma unidad logica?
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