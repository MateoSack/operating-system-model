#ifndef CPU_MANAGE_INSTRUCTIONS_H
#define CPU_MANAGE_INSTRUCTIONS_H

#include <utils/net_utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<utils/process_utils.h>

extern pthread_mutex_t interrupt_mutex;
extern pthread_mutex_t kernel_scheduler_write_mutex;
extern pthread_mutex_t kernel_memory_write_mutex;
extern pthread_mutex_t kernel_memory_read_mutex;

typedef enum {
	NO_OP,
	SET,
	MOV_IN,
	MOV_OUT,
	SUM,
	SUB,
	JNZ,
	COPY_MEM,
	INS_MUTEX_CREATE,
	INS_MUTEX_LOCK,
	INS_MUTEX_UNLOCK,
	INS_MEM_ALLOC,
	INS_MEM_FREE,
	INS_SLEEP,
	INS_STDOUT,
	INS_STDIN,
	INS_INIT_PROC,
	INS_EXIT,
	UNKNOWN
} t_instruction_type; //FALTAN SYSCALLS

typedef struct {
	uint32_t pid;
	t_cpu_context *context;
} t_process_execution_args;

typedef struct {
	void *field_address;
	size_t field_size;
} t_register_descriptor;

extern t_log *logger;
extern int kernel_memory_fd;
extern int kernel_scheduler_fd;
extern bool interruptPending;

void instructions_cicle(t_cpu_context *context, uint32_t pid);
t_instruction_type instruction_to_type(char *instruction_str);
char **decode_instruction(char *content);
void execute_instruction(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped);
void instruction_set(char **decoded_instruction, t_cpu_context *context);
void instruction_sum(char **decoded_instruction, t_cpu_context *context);
void instruction_sub(char **decoded_instruction, t_cpu_context *context);
void instruction_jnz(char **decoded_instruction, t_cpu_context *context, bool *hasJumped);
void *process_execution_handler(void *args);
bool check_if_register(char *operand);

t_register_descriptor get_register_descriptor(t_cpu_context *context, const char *register_name);
uint32_t read_register_value(t_cpu_context *context, const char *register_name);
bool write_register_value(t_cpu_context *context, const char *register_name, uint32_t value);

void instruction_set(char **decoded_instruction, t_cpu_context *context);
void instruction_sum(char **decoded_instruction, t_cpu_context *context);
void instruction_sub(char **decoded_instruction, t_cpu_context *context);
void instruction_jnz(char **decoded_instruction, t_cpu_context *context, bool *hasJumped);
void instruction_mov_in(char **decoded_instruction, t_cpu_context *context);
void instruction_mov_out(char **decoded_instruction, t_cpu_context *context);
void instruction_copy_mem(char **decoded_instruction, t_cpu_context *context);
void instruction_mutex_create(char **decoded_instruction, t_cpu_context *context);
void instruction_mutex_lock(char **decoded_instruction, t_cpu_context *context);
void instruction_mutex_unlock(char **decoded_instruction, t_cpu_context *context);
void instruction_mem_alloc(char **decoded_instruction, t_cpu_context *context, uint32_t pid);
void instruction_mem_free(char **decoded_instruction, t_cpu_context *context, uint32_t pid);
void instruction_sleep(char **decoded_instruction, t_cpu_context *context, uint32_t pid);
void instruction_stdout(char **decoded_instruction, t_cpu_context *context, uint32_t pid);
void instruction_stdin(char **decoded_instruction, t_cpu_context *context, uint32_t pid);
void instruction_init_proc(char **decoded_instruction, t_cpu_context *context, uint32_t pid);
void instruction_exit(char **decoded_instruction, t_cpu_context *context, uint32_t pid);

#endif // CPU_MANAGE_INSTRUCTIONS_H