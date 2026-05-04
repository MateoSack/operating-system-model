#ifndef UTILS_H_
#define UTILS_H_

#include <utils/server_utils.h>
#include <utils/process_utils.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

typedef struct {
    uint32_t pid;
    t_process_state state;
    uint8_t priority;
} t_process;

void process_set_state (t_process *process, t_process_state state, t_log *logger);
t_process *add_process_to_list (t_list *list_processes, uint32_t pid, uint8_t priority);
void send_process_create_info (uint32_t pid, char *path, int kernel_memory_fd);

#endif
