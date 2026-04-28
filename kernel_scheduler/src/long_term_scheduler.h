#include <utils.h>

int long_term_scheduler (t_log *logger, t_list *list_processes, uint32_t *current_pid, char *path, uint8_t priority, int kernel_memory_fd);
uint32_t pid_assigner (uint32_t *current_pid);