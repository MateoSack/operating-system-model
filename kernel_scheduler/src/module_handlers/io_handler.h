#ifndef IO_HANDLER_H
#define IO_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>
#include <utils.h>

extern pthread_mutex_t io_id_mutex;

extern t_list *list_cpu;
extern t_list *list_io;
extern t_list *list_processes;

extern t_log *logger;
extern int kernel_memory_fd;

extern uint32_t next_io_id;

void io_handler (int io_fd);

#endif
