#ifndef KERNEL_MEMORY_HANDLER_H
#define KERNEL_MEMORY_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>
#include <managers/memory_manager.h>
#include <managers/io_manager.h>
#include <schedulers/long_term_scheduler.h>

extern t_list *list_cpu;
extern t_list *list_processes;

extern t_log *logger;
extern t_client_info *kernel_memory;

extern sem_t compaction_finished_sem;

int kernel_memory_connection (t_log *logger, t_config *config, char *process0);
void *kernel_memory_handler ();
void receive_confirmation_io_memory_write (uint32_t *pid, bool *write_succesful);
void receive_read_value_io_memory_read (uint32_t *pid, char **value);

#endif
