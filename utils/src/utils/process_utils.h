#ifndef UTILS_PROCESS_UTILS_H_
#define UTILS_PROCESS_UTILS_H_

#include <utils/net_utils.h>
#include <utils/memory_utils.h>

typedef enum {
    NEW,
    READY,
    EXEC,
    BLOCK,
    SUSP_BLOCK,
    SUSP_READY,
    EXIT,
} t_process_state;

typedef enum {
    QUANTUM_EXPIRED,
    PRIORITY_PREEMPTION,
    CORRUPT_MEMORY,
    MUTEX_LOCKED,
    IO_REQUEST,
    PROCESS_EXIT,
    COMPACTION,
} t_interrupt_reason;

typedef struct {
    uint32_t pc;
	uint8_t ax;
	uint8_t bx;
    uint8_t cx;
    uint8_t dx;
	uint32_t eax;
	uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t si;
    uint32_t di;
} t_cpu_context;

// `t_segment` is defined in <utils/memory_utils.h>

typedef struct {
    uint32_t pid;
    uint32_t value;
    t_io_type io_type;
} t_io_numeric_process;

typedef struct {
    uint32_t pid;
    char *value;
    t_io_type io_type;
} t_io_string_process;

void context_send_with_pid(t_cpu_context *context, uint32_t pid, int fd, pthread_mutex_t *mutex);
void context_send (t_cpu_context *context, int client_socket, pthread_mutex_t *mutex);
t_cpu_context *context_receive(int client_socket);
t_process_state t_process_state_deserialize(void *buffer, int *offset);
const char* process_state_to_string(t_process_state state);
const char* interrupt_reason_to_string(t_interrupt_reason reason);
void io_numeric_process_send (t_io_numeric_process *io_process, op_code op_code, int client_socket, pthread_mutex_t *mutex);
t_io_numeric_process *io_numeric_process_receive(int client_socket);
void io_string_process_send (t_io_string_process *io_process, op_code op_code, int client_socket, pthread_mutex_t *mutex);
t_io_string_process *io_string_process_receive(int client_socket);
t_io_numeric_process *t_io_numeric_process_create(uint32_t pid, uint32_t value, t_io_type io_type);
t_io_string_process *t_io_string_process_create(uint32_t pid, char *value, t_io_type io_type);
t_interrupt_reason t_interrupt_reason_deserialize(void *buffer, int *offset);

#endif
