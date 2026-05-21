#include <utils/net_utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<utils/process_utils.h>
#include<instructions.h>

typedef enum {
	NO_OP,
	SET,
	MOV_IN,
	MOV_OUT,
	SUM,
	SUB,
	JNZ,
	COPY_MEM,
	UNKNOWN
} t_instruction_type; //FALTAN SYSCALLS

typedef struct {
	uint32_t pid;
	t_cpu_context *context;
} t_process_execution_args;

void end_program(int, t_log*, t_config*);
t_log *start_logger(t_config *config);
int connect_kernel_memory (t_log *logger, t_config *config);
int connect_kernel_scheduler(t_log *logger, t_config *config);
void *kernel_memory_thread ();
void kernel_scheduler_handler (int kernel_scheduler_fd, int kernel_memory_fd);
int iterate_connection_create_with_memory_sticks (t_list *list);
int connect_with_memory_stick (t_log *logger, t_module_credentials *credentials);
void *memory_stick_handler (void *mem_stick_ptr);
