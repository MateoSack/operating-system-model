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
    void *data;
    uint32_t size;
    bool ready;
    pthread_mutex_t mutex;
    sem_t sem;
} t_swap_read_response;

extern uint32_t target_pid;
extern t_client_info *kernel_scheduler;
extern pthread_mutex_t list_processes_mutex;
extern t_list *list_processes;

t_pcb *create_pcb(uint32_t pid, char *path);
bool find_by_pid(void *element);
char **get_instructions_from_file(char *path);
void send_memory_update(uint32_t new_total);
t_memory_stick_info *create_memory_stick_info(int fd, uint32_t id);
int process_get_segment_count(uint32_t pid);

#endif
