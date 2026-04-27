#ifndef KERNEL_SCHEDULER_HANDLER_H
#define KERNEL_SCHEDULER_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>

int kernel_scheduler_handler(t_log *logger, int client_fd);

#endif