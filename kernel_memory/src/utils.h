#ifndef UTILS_H_
#define UTILS_H_

#include <utils/server_utils.h>

typedef struct {
    uint32_t pid;
    t_cpu_context context;
    char *instruction_path;
    t_list *segment_table;
} t_pcb;

t_pcb *create_pcb(uint32_t pid, char *path);

#endif
