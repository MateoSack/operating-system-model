#ifndef MEMORY_MANAGE_H
#define MEMORY_MANAGE_H

#include <main.h>
#include <utils/memory_utils.h>

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

#endif