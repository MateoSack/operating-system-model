#include <utils/process_utils.h>

void context_send (t_cpu_context *context, int client_socket) { // Sends the CPU context to the client
    t_package *package = package_create();
	package->op_code = CONTEXT_TRANSFER;
    package_add(package, &context->pc, sizeof(uint32_t));
    package_add(package, &context->ax, sizeof(uint8_t));
	package_add(package, &context->bx, sizeof(uint8_t));
	package_add(package, &context->cx, sizeof(uint8_t));
	package_add(package, &context->dx, sizeof(uint8_t));
    package_add(package, &context->eax, sizeof(uint32_t));
	package_add(package, &context->ebx, sizeof(uint32_t));
	package_add(package, &context->ecx, sizeof(uint32_t));
	package_add(package, &context->edx, sizeof(uint32_t));
    package_add(package, &context->si, sizeof(uint32_t));
    package_add(package, &context->di, sizeof(uint32_t));
    package_send(package, client_socket);
    package_delete(package);
}

t_cpu_context *context_receive(int client_socket) { // Receives the CPU context from the client
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, client_socket);
    t_cpu_context *context = malloc(sizeof(t_cpu_context));

	context->pc = uint32_deserialize(buffer, &offset);
	context->ax = uint8_deserialize(buffer, &offset);
	context->bx = uint8_deserialize(buffer, &offset);
	context->cx = uint8_deserialize(buffer, &offset);
	context->dx = uint8_deserialize(buffer, &offset);
    context->eax = uint32_deserialize(buffer, &offset);
    context->ebx = uint32_deserialize(buffer, &offset);
    context->ecx = uint32_deserialize(buffer, &offset);
	context->edx = uint32_deserialize(buffer, &offset);
    context->si = uint32_deserialize(buffer, &offset);
    context->di = uint32_deserialize(buffer, &offset);

    free(buffer);
    return context;
}

t_process_state t_process_state_deserialize(void *buffer, int *offset) { // Deserializes a t_process_state from a buffer, updating the offset
	int size;
	t_module_id value;
	memcpy(&size, buffer + *offset, sizeof(t_process_state));
	*offset += sizeof(t_process_state);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

const char* process_state_to_string(t_process_state state) { // Converts a t_process_state to a string, for logging purposes
    switch(state) {
        case NEW: return "NEW";
        case READY: return "READY";
        case EXEC: return "EXEC";
        case BLOCK: return "BLOCK";
        case SUSP_BLOCK: return "SUSP_BLOCK";
        case SUSP_READY: return "SUSP_READY";
        case EXIT: return "EXIT";
        default: return "UNKNOWN";
    }
}

const char* interrupt_reason_to_string(t_interrupt_reason reason) { // Converts a t_interrupt_reason to a string, for logging purposes
    switch(reason) {
        case QUANTUM_EXPIRATION: return "QUANTUM_EXPIRATION";
        case HIGHER_PRIORITY: return "HIGHER_PRIORITY";
        case CORRUPT_MEMORY: return "CORRUPT_MEMORY";
        case MUTEX_LOCKED: return "MUTEX_LOCKED";
        default: return "UNKNOWN";
    }
}
