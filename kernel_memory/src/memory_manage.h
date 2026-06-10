#ifndef MEMORY_MANAGE_H
#define MEMORY_MANAGE_H

#include <main.h>
#include <utils/memory_utils.h>

t_list *get_free_holes(void);
t_segment_result segment_create(uint32_t pid, uint32_t segment_id, uint32_t size, t_config *config);

#endif