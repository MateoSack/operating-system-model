#ifndef LONG_TERM_SCHEDULER_H_
#define LONG_TERM_SCHEDULER_H_

#include <utils.h>
#include <schedulers/short_term_scheduler.h>

extern t_log *logger;
extern t_client_info *kernel_memory;
extern t_list *list_processes;
extern t_list **ready_queue;
extern uint32_t current_max_pid;
extern pthread_mutex_t scheduler_mutex;
extern sem_t short_term_scheduler_sem;

int long_term_scheduler (char *path, uint8_t priority);
void wait_process_create_confirmation (uint32_t pid);
uint32_t pid_assigner (uint32_t *current_pid);

#endif
