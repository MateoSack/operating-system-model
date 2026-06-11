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
extern t_client_info *kernel_memory;

extern uint32_t next_io_id;

void io_handler (int io_fd);
void io_finish_process(uint32_t pid, t_client_info *io);
void handle_next_operation (t_client_info *io, t_io_type io_type, t_list *pending_io_list, op_code op_code);

#endif
