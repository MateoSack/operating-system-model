#ifndef UTILS_H_
#define UTILS_H_

#include <utils/server_utils.h>
#include <utils/process_utils.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <semaphore.h>

typedef struct {
    uint32_t pid;
    t_process_state state;
    uint8_t base_priority;
    uint8_t effective_priority;
    t_client_info *cpu;
    uint64_t start_exec_time;
} t_process;

typedef enum {
    FIFO,
    RR,
    CMN,
} t_scheduler_algorithm;

extern t_log *logger;
extern t_scheduler_algorithm scheduler_algorithm;
extern t_scheduler_algorithm *queue_algorithms;
extern int queue_algorithms_count;
extern pthread_mutex_t scheduler_mutex;
extern t_list *list_processes;
extern t_list **ready_queue;
extern t_list *exec_processes;
extern sem_t short_term_scheduler_sem;

void process_set_state (t_process *process, t_process_state state, t_log *logger);
void process_set_cpu (t_process *process, t_client_info *cpu);
t_process *create_process (uint32_t pid, uint8_t base_priority);
void add_process_to_list (t_list *list_processes, t_process *process);
void add_process_to_ready_queue (t_process *process);
void remove_process_from_list (t_list *list_processes, t_process *process);
void remove_process_from_ready_queue (t_process *process);
void destroy_list_of_processes (t_list *list);
int ready_queue_size ();
void send_process_create_info (uint32_t pid, char *path, int kernel_memory_fd, pthread_mutex_t *mutex);
t_scheduler_algorithm scheduler_algorithm_from_string(const char *str);
bool process_has_quantum (t_process *process);
t_process *get_process_from_pid (uint32_t pid);
t_process *get_process_from_cpu (t_client_info *cpu);
t_process *get_highest_priority_process_from_ready_queue ();
t_process *get_lowest_priority_process (t_list *process_list);
void evict_process(t_client_info *cpu, t_interrupt_reason reason, bool should_handle_state);
void *wait_confirmation_thread_and_handle_state (void *arg);
void *wait_confirmation_thread (void *arg);
void evict_all_processes (t_interrupt_reason reason);
t_client_info *get_available_io_type (t_list *io_list);
t_io_numeric_process *get_next_io_numeric_process_from_list(t_list *io_pending_list);
t_io_string_process *get_next_io_string_process_from_list(t_list *io_pending_list);

#endif
