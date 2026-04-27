#ifndef UTILS_H_
#define UTILS_H_

#include <utils/server_utils.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

typedef struct {
    uint32_t pid;
    t_process_state state;
    uint8_t priority;
} t_process;

void pcb_set_state (t_pcb *pcb, t_process_state state, t_log *logger);
void add_process_to_list (t_list *list_processes, uint32_t pid, uint8_t priority);

#endif
