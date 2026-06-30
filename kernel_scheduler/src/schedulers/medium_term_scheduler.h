#ifndef MEDIUM_TERM_SCHEDULER_H_
#define MEDIUM_TERM_SCHEDULER_H_

#include <utils.h>
#include <commons/temporal.h>

extern t_log *logger;
extern t_client_info *kernel_memory;
extern t_temporal *system_timer;
extern int suspension_timeout;
extern t_list *list_processes;
extern t_list **ready_queue;
extern t_list *block_processes;
extern t_list *suspended_processes;
extern t_scheduler_algorithm scheduler_algorithm;
extern pthread_mutex_t scheduler_mutex;
extern pthread_mutex_t block_processes_mutex;
extern pthread_mutex_t suspended_processes_mutex;
extern sem_t short_term_scheduler_sem;

#endif
