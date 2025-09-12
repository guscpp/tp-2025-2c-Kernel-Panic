#ifndef SUELTOS_H_
#define SUELTOS_H_

#include <commons/log.h>     // Para t_log
#include <commons/config.h>  // Para t_config
#include <stdlib.h>          // Para exit

void terminar_programa(t_log *logger, t_config *config);
t_config *iniciar_config(t_log *logger, char *path); 
t_log *iniciar_logger(char *logger_name, char *process_name, bool visible, t_log_level log_level);

#endif