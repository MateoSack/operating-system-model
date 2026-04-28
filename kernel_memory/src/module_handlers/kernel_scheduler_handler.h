#ifndef KERNEL_SCHEDULER_HANDLER_H
#define KERNEL_SCHEDULER_HANDLER_H

#include <utils.h>
#include <commons/config.h>

int kernel_scheduler_handler(t_log *logger, int client_fd, t_config *config);

#endif