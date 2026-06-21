#ifndef CPU_MANAGE_INSTRUCTIONS_H
#define CPU_MANAGE_INSTRUCTIONS_H

#include<utils/net_utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<utils/process_utils.h>
#include<semaphore.h>
#include<main.h>

extern pthread_mutex_t interrupt_mutex;
extern pthread_mutex_t process_control_mutex;
extern pthread_mutex_t memory_stick_list_mutex;
extern sem_t sem_instruction_fetch_ready;
extern sem_t sem_instruction_response_ready;
extern sem_t sem_eviction_ready;

typedef enum {
	NOOP,
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
	void *field_address;
	size_t field_size;
} t_register_descriptor;

typedef struct {
	char *instruction;
	bool is_ready;
	pthread_mutex_t mutex;
} t_instruction_response;

extern t_log *logger;
extern t_client_info *kernel_memory;
extern t_client_info *kernel_scheduler;
extern bool interruptPending;
extern t_interrupt_reason interruptReason;
extern t_interrupt_reason stopReason;
extern t_instruction_response instruction_response;
extern uint32_t segment_max_size;
extern t_list *list_memory_stick;
extern bool is_executing;

void context_send(t_cpu_context *context, uint32_t pid, int fd, pthread_mutex_t *mutex);
void instructions_cicle(t_cpu_context *context, uint32_t pid, t_list *segment_table);
t_instruction_type instruction_to_type(char *instruction_str);
char **decode_instruction(char *content);
void execute_instruction(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table, bool *hasJumped);
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
void instruction_mov_in(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table);
void instruction_mov_out(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table);
void instruction_copy_mem(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table);
void instruction_mutex_create(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped);
void instruction_mutex_lock(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped);
void instruction_mutex_unlock(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped);
void instruction_mem_alloc(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped);
void instruction_mem_free(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped);
void instruction_sleep(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped);
void instruction_stdout(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table, bool *hasJumped);
void instruction_stdin(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table, bool *hasJumped);
void instruction_init_proc(char **decoded_instruction, t_cpu_context *context, uint32_t pid);
void instruction_exit(char **decoded_instruction, t_cpu_context *context, uint32_t pid);

uint32_t mmu_translate(uint32_t logical_address, uint32_t size, t_list *segment_table, uint32_t pid);
t_memory_stick_info *get_memory_stick_by_address(uint32_t physical_address, uint32_t *local_offset);
void *memory_read(uint32_t physical_address, uint32_t size);
bool memory_write(uint32_t physical_address, void *data, uint32_t size);
void send_process_interrupted(uint32_t pid, t_interrupt_reason reason);

#endif // CPU_MANAGE_INSTRUCTIONS_H
