#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <commons/log.h>
#include <utils/server_utils.h>
#include <utils.h>

extern t_log *logger;
extern t_client_info *kernel_memory;
extern sem_t compaction_finished_sem;

void receive_instruction_mem_alloc (uint32_t *pid, uint32_t *segment_id, uint32_t *segment_size, int cpu_fd);
void mem_alloc_syscall_manager(uint32_t pid, uint32_t segment_id, uint32_t segment_size);
void receive_instruction_mem_free (uint32_t *pid, uint32_t *segment_id, int cpu_fd);
void mem_free_syscall_manager(uint32_t pid, uint32_t segment_id);
void compaction_requested ();
void *compaction_requested_thread (void *arg);
void memory_corrupted();
void *memory_corrupted_thread (void *arg);
void memory_update (uint32_t new_size);

#endif
