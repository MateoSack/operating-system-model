#ifndef KERNEL_MEMORY_HANDLER_H
#define KERNEL_MEMORY_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>

extern t_list *list_cpu;
extern t_list *list_processes;

extern t_log *logger;
extern int kernel_memory_fd;

void *kernel_memory_handler ();

#endif
