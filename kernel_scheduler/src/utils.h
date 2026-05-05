#ifndef UTILS_H_
#define UTILS_H_

#include <utils/server_utils.h>
#include <utils/process_utils.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

extern t_list *list_processes;

typedef struct {
    uint32_t pid;
    t_process_state state;
    uint8_t priority;
    t_client_info *cpu;
} t_process;

typedef enum {
    FIFO,
    RR,
    CMN,
} t_scheduler_algorithm;

void process_set_state (t_process *process, t_process_state state, t_log *logger);
void process_set_cpu (t_process *process, t_client_info *cpu);
t_process *create_process (uint32_t pid, uint8_t priority);
void add_process_to_list (t_list *list_processes, t_process *process);
void remove_process_from_list (t_list *list_processes, t_process *process);
void send_process_create_info (uint32_t pid, char *path, int kernel_memory_fd);
t_scheduler_algorithm scheduler_algorithm_from_string(const char *str);
t_process *get_process_from_pid (uint32_t pid);
t_process *get_process_by_cpu (t_client_info *cpu);

#endif
