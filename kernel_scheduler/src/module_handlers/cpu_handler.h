#ifndef CPU_HANDLER_H
#define CPU_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>
#include <schedulers/long_term_scheduler.h>
#include <schedulers/short_term_scheduler.h>
#include <managers/mutex_manager.h>
#include <managers/io_manager.h>
#include <managers/memory_manager.h>
#include <utils.h>

extern t_scheduler_algorithm scheduler_algorithm;

extern sem_t short_term_scheduler_sem;

extern pthread_mutex_t cpu_id_mutex;

extern t_list *list_cpu;
extern t_list *list_processes;
extern t_list **ready_queue;

extern t_log *logger;
extern t_client_info *kernel_memory;

extern uint32_t next_cpu_id;

void cpu_handler (int cpu_fd);
void handle_cpu_disconnection (t_client_info *cpu);
void receive_instruction_process_create (uint32_t *pid, uint32_t *priority, char **path, int cpu_fd);
t_client_info *get_available_io_type (t_list *io_list);
void receive_interruption (uint32_t *pid, t_interrupt_reason *reason, int cpu_fd);
void send_pid_with_op_code (uint32_t pid, op_code op_code, int client_socket, pthread_mutex_t *mutex);
void receive_instruction_mem_alloc (uint32_t *pid, uint32_t *segment_id, uint32_t *size, int cpu_fd);

#endif
