#ifndef UTILS_H_
#define UTILS_H_

#include <utils/server_utils.h>
#include <utils/process_utils.h>

typedef struct {
    uint32_t pid;
    t_cpu_context context;
    char *instruction_path;
    t_list *segment_table;
} t_pcb;

extern uint32_t target_pid;

t_pcb *create_pcb(uint32_t pid, char *path);
bool find_by_pid(void *element);

#endif
