#ifndef KERNEL_SCHEDULER_HANDLER_H
#define KERNEL_SCHEDULER_HANDLER_H

#include <utils.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <utils/memory_utils.h>
#include <memory_manage.h>

extern t_client_info *kernel_scheduler;

int kernel_scheduler_handler(t_log *logger, int client_fd, t_config *config);

#endif