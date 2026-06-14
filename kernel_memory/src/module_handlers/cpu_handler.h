#ifndef CPU_HANDLER_H
#define CPU_HANDLER_H

#include <utils.h>

int cpu_handler(t_log *logger, t_client_info *cpu);
void context_send_from_pcb(t_pcb *pcb, uint32_t pid, int fd, pthread_mutex_t *mutex);

#endif