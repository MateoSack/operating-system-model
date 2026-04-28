#ifndef CPU_HANDLER_H
#define CPU_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>
#include <long_term_scheduler.h>

extern t_list *list_cpu;
extern t_list *list_io;
extern t_list *list_processes;

extern t_log *logger;
extern int kernel_memory_fd;

extern uint32_t next_cpu_id;

void cpu_handler (int cpu_fd);

#endif
