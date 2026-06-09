#ifndef UTILS_H_
#define UTILS_H_

#include <utils/server_utils.h>
#include <utils/process_utils.h>

typedef struct {
    uint32_t pid;
    t_cpu_context context;
    char **instructions;
    t_list *segment_table;
} t_pcb;

typedef struct {
    int fd;
    uint32_t id;
    uint32_t size;
    uint32_t base_address; // CHEQUEAR SI ES NECESARIO
} t_memory_stick_info;

extern uint32_t target_pid;

t_pcb *create_pcb(uint32_t pid, char *path);
bool find_by_pid(void *element);
char **get_instructions_from_file(char *path);
void send_memory_update(int ks_fd, uint32_t new_total);

#endif
