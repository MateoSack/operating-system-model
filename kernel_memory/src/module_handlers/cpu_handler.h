#ifndef CPU_HANDLER_H
#define CPU_HANDLER_H

#include <utils.h>

int cpu_handler(t_log *logger, int client_fd, int cpu_id);
bool find_by_pid(void *element);

#endif