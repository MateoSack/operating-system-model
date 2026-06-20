#ifndef SHORT_TERM_SCHEDULER_H_
#define SHORT_TERM_SCHEDULER_H_

#include <utils.h>
#include <commons/temporal.h>

extern t_log *logger;
extern t_client_info *kernel_memory;
extern int quantum;
extern bool queue_preemption;
extern t_temporal *system_timer;
extern t_list *list_processes;
extern t_list **ready_queue;
extern t_list *exec_processes;
extern t_list *list_cpu;
extern t_scheduler_algorithm scheduler_algorithm;
extern pthread_mutex_t scheduler_mutex;
extern sem_t short_term_scheduler_sem;

void *short_term_scheduler_main ();
t_process *get_next_process_to_execute ();
t_client_info *get_available_cpu();
void *quantum_manager ();

#endif
