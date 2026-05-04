#ifndef SHORT_TERM_SCHEDULER_H_
#define SHORT_TERM_SCHEDULER_H_

#include <utils.h>

extern t_log *logger;
extern int kernel_memory_fd;
extern t_list *list_processes;
extern t_list *list_cpu;
extern t_scheduler_algorithm scheduler_algorithm;

int short_term_scheduler (void);
t_process *get_next_process_to_exec(t_list *list_processes);

#endif
