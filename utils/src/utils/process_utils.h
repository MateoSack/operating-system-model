#ifndef UTILS_PROCESS_UTILS_H_
#define UTILS_PROCESS_UTILS_H_

#include <utils/net_utils.h>

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
    HIGHER_PRIORITY,
    CORRUPT_MEMORY,
    MUTEX_LOCKED,
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

typedef struct {
    uint32_t segment_id;
    uint32_t base;
    uint32_t limit;
} t_segment;

void context_send (t_cpu_context *context, int client_socket);
t_cpu_context *context_receive(int client_socket);
t_process_state t_process_state_deserialize(void *buffer, int *offset);
const char* process_state_to_string(t_process_state state);

#endif
