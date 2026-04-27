#ifndef CPU_HANDLER_H
#define CPU_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>

int cpu_handler(t_log *logger, int client_fd, int cpu_id);

#endif