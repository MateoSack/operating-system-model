#ifndef SHORT_TERM_SCHEDULER_H_
#define SHORT_TERM_SCHEDULER_H_

#include <utils.h>
#include<commons/temporal.h>

extern t_log *logger;
extern int kernel_memory_fd;
extern int quantum;
extern t_temporal *system_timer;
extern t_list *list_processes;
extern t_list *ready_queue;
extern t_list *exec_processes;
extern t_list *list_cpu;
extern t_scheduler_algorithm scheduler_algorithm;
extern pthread_mutex_t scheduler_mutex;
extern sem_t short_term_scheduler_sem;

void *short_term_scheduler_main ();
t_process *get_next_process_to_execute ();
t_client_info *get_available_cpu();
void send_pid_to_execute (uint32_t pid, int cpu_fd);
void *quantum_manager ();

#endif
