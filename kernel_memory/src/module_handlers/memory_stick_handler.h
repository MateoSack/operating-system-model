#ifndef MEMORY_STICK_HANDLER_H
#define MEMORY_STICK_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>

int memory_stick_handler(t_log *logger, int client_fd, t_module_credentials *client);

#endif