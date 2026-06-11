#include <utils/process_utils.h>

void context_send_with_pid(t_cpu_context *context, uint32_t pid, int fd, pthread_mutex_t *mutex) {
    t_package *pkg = package_create();
    pkg->op_code = CONTEXT_TRANSFER;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &context->pc, sizeof(uint32_t));
    package_add(pkg, &context->ax, sizeof(uint8_t));
    package_add(pkg, &context->bx, sizeof(uint8_t));
    package_add(pkg, &context->cx, sizeof(uint8_t));
    package_add(pkg, &context->dx, sizeof(uint8_t));
    package_add(pkg, &context->eax, sizeof(uint32_t));
    package_add(pkg, &context->ebx, sizeof(uint32_t));
    package_add(pkg, &context->ecx, sizeof(uint32_t));
    package_add(pkg, &context->edx, sizeof(uint32_t));
    package_add(pkg, &context->si, sizeof(uint32_t));
    package_add(pkg, &context->di, sizeof(uint32_t));
    package_send(pkg, fd, mutex);
    package_delete(pkg);
}

void context_send (t_cpu_context *context, int client_socket, pthread_mutex_t *mutex) { // Sends the CPU context to the client
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
    package_send(package, client_socket, mutex);
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
        case QUANTUM_EXPIRED: return "QUANTUM_EXPIRED";
        case PRIORITY_PREEMPTION: return "PRIORITY_PREEMPTION";
        case CORRUPT_MEMORY: return "CORRUPT_MEMORY";
        case MUTEX_LOCKED: return "MUTEX_LOCKED";
        case IO_REQUEST: return "IO_REQUEST";
        case PROCESS_EXIT: return "PROCESS_EXIT";
        default: return "UNKNOWN";
    }
}

void io_numeric_process_send (t_io_numeric_process *io_process, op_code op_code, int client_socket, pthread_mutex_t *mutex) { // Sends an IO process with a numeric value request
    t_package *package = package_create();
    package->op_code = op_code;
    package_add(package, &io_process->pid, sizeof(uint32_t));
    package_add(package, &io_process->value, sizeof(uint32_t));
    package_add(package, &io_process->io_type, sizeof(t_io_type));

    package_send(package, client_socket, mutex);
    package_delete(package);

    log_debug(logger, "Sending IO Numeric Process - PID: %d, Value: %d, IO Type: %d", io_process->pid, io_process->value, io_process->io_type);
}

t_io_numeric_process *io_numeric_process_receive(int client_socket) { // Receives an IO process request with a numeric value
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, client_socket);
    if (buffer == NULL) return NULL;

    t_io_numeric_process *io_process = malloc(sizeof(t_io_numeric_process));

    io_process->pid = uint32_deserialize(buffer, &offset);
    io_process->value = uint32_deserialize(buffer, &offset);
    io_process->io_type = t_io_type_deserialize(buffer, &offset);

    log_debug(logger, "Deserialized IO Process - PID: %d, Value: %d, IO Type: %d", io_process->pid, io_process->value, io_process->io_type);

    free(buffer);
    return io_process;
}

void io_string_process_send (t_io_string_process *io_process, op_code op_code, int client_socket, pthread_mutex_t *mutex) { // Sends an IO process request with a string value
    t_package *package = package_create();
    package->op_code = op_code;
    package_add(package, &io_process->pid, sizeof(uint32_t));
    uint32_t value_length = strlen(io_process->value) + 1; // +1 for null terminator
    package_add(package, &value_length, sizeof(uint32_t));
    package_add(package, io_process->value, value_length);
    package_add(package, &io_process->io_type, sizeof(t_io_type));
    package_send(package, client_socket, mutex);
    package_delete(package);
}

t_io_string_process *io_string_process_receive(int client_socket) { // Receives an IO process request with a string value
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, client_socket);
    if (buffer == NULL) return NULL;

    t_io_string_process *io_process = malloc(sizeof(t_io_string_process));

    io_process->pid = uint32_deserialize(buffer, &offset);
    uint32_t value_length = uint32_deserialize(buffer, &offset);
    io_process->value = malloc(value_length);
    memcpy(io_process->value, buffer + offset, value_length);
    offset += value_length;
    io_process->io_type = t_io_type_deserialize(buffer, &offset);

    free(buffer);
    return io_process;
}

t_io_numeric_process *t_io_numeric_process_create(uint32_t pid, uint32_t value, t_io_type io_type) { // Creates a t_io_numeric_process with the given values, returns the created process
    t_io_numeric_process *io_process = malloc(sizeof(t_io_numeric_process));
    io_process->pid = pid;
    io_process->value = value;
    io_process->io_type = io_type;

    return io_process;
}

t_io_string_process *t_io_string_process_create(uint32_t pid, char *value, t_io_type io_type) { // Creates a t_io_string_process with the given values, returns the created process
    t_io_string_process *io_process = malloc(sizeof(t_io_string_process));
    io_process->pid = pid;
    io_process->value = strdup(value);
    io_process->io_type = io_type;

    return io_process;
}

t_interrupt_reason t_interrupt_reason_deserialize(void *buffer, int *offset) { // Deserializes a t_interrupt_reason from a buffer, updating the offset
	int size;
	t_interrupt_reason value;
	memcpy(&size, buffer + *offset, sizeof(t_interrupt_reason));
	*offset += sizeof(t_interrupt_reason);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

