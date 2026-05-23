#ifndef IO_HANDLER_H
#define IO_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>
#include <utils.h>

extern pthread_mutex_t io_id_mutex;
extern pthread_mutex_t io_mutex;
extern pthread_mutex_t scheduler_mutex;

extern t_list *list_cpu;
extern t_list *list_io_sleep;
extern t_list *list_io_stdin;
extern t_list *list_io_stdout;
extern t_list *list_processes;

extern sem_t short_term_scheduler_sem;

extern t_list *pending_request_io_sleep;
extern t_list *pending_request_io_stdin;
extern t_list *pending_request_io_stdout;

extern t_log *logger;
extern int kernel_memory_fd;

extern uint32_t next_io_id;

void io_handler (int io_fd);
void io_finish_process(uint32_t pid, t_client_info *io);

#endif
