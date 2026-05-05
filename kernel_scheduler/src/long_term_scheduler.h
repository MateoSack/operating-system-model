#ifndef LONG_TERM_SCHEDULER_H_
#define LONG_TERM_SCHEDULER_H_

#include <utils.h>
#include <short_term_scheduler.h>

extern t_log *logger;
extern int kernel_memory_fd;
extern t_list *list_processes;
extern t_list *ready_queue;
extern uint32_t current_max_pid;
extern pthread_mutex_t scheduler_mutex;

int long_term_scheduler (char *path, uint8_t priority);
uint32_t pid_assigner (uint32_t *current_pid);

#endif
