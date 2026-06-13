#ifndef UTILS_H_
#define UTILS_H_

#include <utils/server_utils.h>
#include <utils/process_utils.h>

typedef struct {
    uint32_t pid;
    t_cpu_context context;
    char **instructions;
    t_list *segment_table;
    pthread_mutex_t mutex; // Mutex for synchronizing access to the PCB, always lock before unlocking list_processes_mutex to avoid sync issues
} t_pcb;

typedef struct {
    int fd;
    uint32_t id;
    uint32_t size;
    uint32_t base_address;
    pthread_mutex_t mutex;
    pthread_mutex_t network_mutex;
    sem_t response_sem;
    int last_op_result;
    void *last_read_buffer;
    int last_read_size;
} t_memory_stick_info;

extern uint32_t target_pid;
extern t_client_info *kernel_scheduler;

t_pcb *create_pcb(uint32_t pid, char *path);
bool find_by_pid(void *element);
char **get_instructions_from_file(char *path);
void send_memory_update(uint32_t new_total);

#endif
