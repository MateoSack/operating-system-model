#ifndef CPU_MAIN_H
#define CPU_MAIN_H

#include <utils/net_utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<utils/process_utils.h>
#include<manage_instructions.h>
#include<semaphore.h>

// Pending request for context seek
typedef struct {
	uint32_t pid;
	t_cpu_context *context;
	t_list *segment_table;
	sem_t sem;
	bool ready;
} t_pending_request;

typedef struct {
	uint32_t pid;
	t_cpu_context *context;
	t_list *segment_table;
} t_process_execution_args;

void end_program(int, t_log*, t_config*);
t_log *start_logger(t_config *config, char *cpu_id);
int connect_kernel_memory(t_log *logger, t_config *config);
int connect_kernel_scheduler(t_log *logger, t_config *config);
void *kernel_memory_handler();
void kernel_scheduler_handler(t_client_info *kernel_scheduler);
int iterate_connection_create_with_memory_sticks (t_list *list);
int connect_with_memory_stick(t_log *logger, t_memory_stick_credentials *credentials);
void *memory_stick_handler(void *mem_stick_ptr);
t_process_execution_args *context_receive(int fd);

#endif // CPU_MAIN_H
