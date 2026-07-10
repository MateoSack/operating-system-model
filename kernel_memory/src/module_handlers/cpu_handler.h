#ifndef CPU_HANDLER_H
#define CPU_HANDLER_H

#include <utils.h>

extern t_list *list_cpu;
extern t_list *list_processes;

extern pthread_mutex_t list_processes_mutex;

extern uint32_t target_pid;
extern uint32_t instruction_delay;

int cpu_handler(t_log *logger, t_client_info *cpu);
void context_send_from_pcb(t_pcb *pcb, uint32_t pid, int fd, pthread_mutex_t *mutex);

#endif