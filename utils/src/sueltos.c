#include "sueltos.h"
#include <commons/log.h>
#include <stdio.h>          // Para printf
#include <stdlib.h>         // Para exit


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
