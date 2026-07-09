#ifndef MEMORY_MANAGE_H
#define MEMORY_MANAGE_H

#include <main.h>
#include <utils/memory_utils.h>

extern uint32_t swap_block_size;
extern uint32_t swap_total_size;
extern int swap_fd;
extern pthread_mutex_t swap_network_mutex;
extern t_list *list_suspended_processes;
extern pthread_mutex_t list_suspended_processes_mutex;
extern t_swap_read_response swap_read_response;
extern sem_t sem_swap_write_done;
extern sem_t swap_read_response_sem;

typedef struct {
    uint32_t segment_id;
    uint32_t size;
    t_list *swap_blocks; // uint32_t* list
} t_suspended_segment;

typedef struct {
    uint32_t pid;
    t_list *suspended_segments; // t_suspended_segment* list
} t_suspended_pcb;

t_list *get_free_holes(void);
t_hole *best_fit(t_list *holes, uint32_t size);
t_hole *worst_fit(t_list *holes, uint32_t size);
t_hole *select_hole(t_list *holes, uint32_t size);
void compact_memory(void);
void request_and_compact(void);
t_segment_result segment_create(uint32_t pid, uint32_t segment_id, uint32_t size);
t_memory_stick_info *get_memory_stick_by_address(uint32_t physical_address, uint32_t *local_offset);
void *memory_read(uint32_t physical_address, uint32_t size);
bool memory_write(uint32_t physical_address, void *data, uint32_t size);
int segment_delete(uint32_t pid, uint32_t segment_id);
void send_segment_result(uint32_t pid, uint32_t segment_id, int result);
bool find_suspended_by_pid(void *element);
t_list *get_free_swap_blocks(uint32_t blocks_needed);
bool write_segment_to_swap(t_suspended_segment *ss, void *data);
void *read_segment_from_swap(t_suspended_segment *ss);
bool process_suspend(uint32_t pid);
bool process_desuspend(uint32_t pid);

#endif