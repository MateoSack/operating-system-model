#ifndef IO_MANAGER_H
#define IO_MANAGER_H

#include <utils.h>

extern t_log *logger;

extern t_client_info *kernel_memory;

extern pthread_mutex_t io_mutex;

extern pthread_mutex_t scheduler_mutex;

extern sem_t short_term_scheduler_sem;

extern t_list *list_io_sleep;
extern t_list *list_io_stdin;
extern t_list *list_io_stdout;

extern t_list *pending_request_io_sleep;
extern t_list *pending_request_io_stdin;
extern t_list *pending_request_io_stdout;

int sleep_syscall_manager (t_process *process, t_client_info *cpu, uint32_t sleep_time);
int stdin_syscall_manager (t_process *process, t_client_info *cpu, uint32_t base, uint32_t limit);
int stdout_syscall_manager (t_process *process, t_client_info *cpu, uint32_t base, uint32_t limit);

#endif
