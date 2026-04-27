#include <utils.h>

int long_term_scheduler (t_log *logger, t_list *list_processes, uint32_t *current_pid, char *path, uint8_t priority, int kernel_memory_fd);
void create_pcb (t_pcb *pcb, uint32_t pid, uint8_t priority);
uint32_t pid_assigner (uint32_t *current_pid);