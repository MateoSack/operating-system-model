#ifndef IO_MANAGER_H
#define IO_MANAGER_H

#include <utils.h>

typedef struct {  // Use for before sending data to kernel memory
    uint32_t pid;
    uint32_t physical_address;
} t_pending_stdin;

extern t_log *logger;

extern t_client_info *kernel_memory;

extern pthread_mutex_t io_mutex;

extern pthread_mutex_t pending_io_stdin_reading_mutex;

extern pthread_mutex_t scheduler_mutex;

extern sem_t short_term_scheduler_sem;

extern t_list *list_io_sleep;
extern t_list *list_io_stdin;
extern t_list *list_io_stdout;

extern t_list *pending_request_io_sleep;
extern t_list *pending_request_io_stdin;
extern t_list *pending_io_stdin_reading;
extern t_list *pending_request_io_stdout;

int sleep_syscall_manager (t_process *process, t_client_info *cpu, uint32_t sleep_time);
int stdin_syscall_manager (t_process *process, t_client_info *cpu, uint32_t physical_address, uint32_t to_read);
void *wait_memory_write_confirmation (void *arg);
t_pending_stdin *get_pending_stdin_from_pid (uint32_t pid);
t_pending_stdin *t_pending_stdin_create (uint32_t pid, uint32_t physical_address);
int stdout_syscall_manager (t_process *process, t_client_info *cpu, uint32_t physical_address, uint32_t to_read);
void stdout_wait_memory_read (uint32_t pid, char *value);
void receive_instruction_sleep (uint32_t *pid, uint32_t *sleep_time, int cpu_fd);
void receive_instruction_std (uint32_t *pid, uint32_t *physical_address, uint32_t *to_read, int cpu_fd);
void io_finish_process(uint32_t pid, t_client_info *io);
void handle_next_operation (t_client_info *io, t_io_type io_type, t_list *pending_io_list, op_code op_code);

#endif
