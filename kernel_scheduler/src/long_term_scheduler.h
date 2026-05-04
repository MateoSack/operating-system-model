#ifndef LONG_TERM_SCHEDULER_H_
#define LONG_TERM_SCHEDULER_H_

#include <utils.h>

extern t_log *logger;
extern int kernel_memory_fd;
extern t_list *list_processes;
extern uint32_t current_max_pid;

int long_term_scheduler (char *path, uint8_t priority);
uint32_t pid_assigner (uint32_t *current_pid);

#endif
