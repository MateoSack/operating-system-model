#include<manage_instructions.h>

bool should_exit = false;
bool should_stop = false;
t_interrupt_reason stop_reason = 0;
bool is_executing = false;

void context_send(t_cpu_context *context, uint32_t pid, int fd, pthread_mutex_t *mutex) {
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

void instructions_cicle(t_cpu_context *context, uint32_t pid, t_list *segment_table) {
    bool hasJumped = false;
	while(1) {
        pthread_mutex_lock(&interrupt_mutex);
        bool interrupt = interruptPending;
        pthread_mutex_unlock(&interrupt_mutex);
        pthread_mutex_lock(&process_control_mutex);
        bool exit_flag = should_exit;
        if(exit_flag) {
            log_debug(logger, "Finalizando proceso PID %d", pid);
            should_exit = false;
            if(interrupt) { // If an interruption was commanded while exiting, we just reset the flag and post the semaphore to unblock eviction handler
                pthread_mutex_lock(&interrupt_mutex);
                interruptPending = false;
                pthread_mutex_unlock(&interrupt_mutex);
                sem_post(&sem_eviction_ready);
            }
            pthread_mutex_unlock(&process_control_mutex);
            send_process_interrupted(pid, PROCESS_EXIT);
        } else {
            pthread_mutex_unlock(&process_control_mutex);
        }

        if(exit_flag) break;
        
        pthread_mutex_lock(&process_control_mutex);
        bool stop_flag = should_stop;
        if(stop_flag) {
            log_debug(logger, "Deteniendo proceso PID %d", pid);
            should_stop = false;

            if(interrupt) { // If an interruption was commanded while stopping, we just reset the flag and post the semaphore to unblock eviction handler
                pthread_mutex_lock(&interrupt_mutex);
                interruptPending = false;
                pthread_mutex_unlock(&interrupt_mutex);
                sem_post(&sem_eviction_ready);
            }
            send_process_interrupted(pid, stop_reason);

        }
        pthread_mutex_unlock(&process_control_mutex);
        if(stop_flag) break;

		t_package *pkg = package_create();
		pkg->op_code = INSTRUCTION_FETCH;
		package_add(pkg, &pid, sizeof(uint32_t));
		package_add(pkg, &context->pc, sizeof(uint32_t));
		
		package_send(pkg, kernel_memory->fd, &kernel_memory->network_mutex);
		package_delete(pkg);
		
		log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid, context->pc);
		
		// Signal kernel_memory_thread that we're ready to receive instruction
		sem_post(&sem_instruction_fetch_ready);
		
		// Wait for kernel_memory_thread to deliver the instruction
		sem_wait(&sem_instruction_response_ready);
        
        log_debug(logger, "sem_instruction_response_ready recibido para PID: %d", pid);

		// Get the instruction from shared structure
		pthread_mutex_lock(&instruction_response.mutex);
		char *instruction = instruction_response.instruction;
		instruction_response.instruction = NULL;
		instruction_response.is_ready = false;
		pthread_mutex_unlock(&instruction_response.mutex);
        
        if (instruction == NULL) {
			log_error(logger, "Failed to receive instruction from Kernel Memory");
			context->pc++;
			break;
		}
		log_info(logger, "Received instruction from Kernel Memory: %s", instruction);
		char **decoded_instruction = decode_instruction(instruction);
        execute_instruction(decoded_instruction, context, pid, segment_table, &hasJumped);
		log_info(logger, "## PID: %d - Ejecutando: %s", pid, decoded_instruction[0]);
		free(instruction);
		string_array_destroy(decoded_instruction);

        if (!hasJumped) {
            context->pc++;
        } else {
            hasJumped = false;
        }

        pthread_mutex_lock(&interrupt_mutex);
        interrupt = interruptPending;
        pthread_mutex_unlock(&interrupt_mutex);

		if(interrupt) {
            log_info(logger, "## Interrupción recibida");

            t_interrupt_reason reason_local;
            pthread_mutex_lock(&interrupt_mutex);
            reason_local = interruptReason;
            pthread_mutex_unlock(&interrupt_mutex);

            log_debug(logger, "Interrupcion pendiente para PID %d con razon %s, enviando contexto a Kernel Memory", pid, interrupt_reason_to_string(reason_local));
            context_send(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
            log_debug(logger, "Contexto enviado a Kernel Memory para PID %d", pid);

            send_process_interrupted(pid, reason_local);

            sem_post(&sem_eviction_ready);

            pthread_mutex_lock(&interrupt_mutex);
            interruptPending = 0;
            pthread_mutex_unlock(&interrupt_mutex);

            break;
		}
		log_info(logger, "No hay interrupciones pendientes para PID %d, continuando ejecucion", pid);
	}
}

char **decode_instruction(char *content) {
	char **decoded_instruction = string_split(content, " ");
	return decoded_instruction;
}

t_instruction_type instruction_to_type(char *instruction_mnemonic) {
	if (strcmp(instruction_mnemonic, "NOOP") == 0)
		return NOOP;
	else if (strcmp(instruction_mnemonic, "SET") == 0)
		return SET;
	else if (strcmp(instruction_mnemonic, "MOV_IN") == 0)
		return MOV_IN;
	else if (strcmp(instruction_mnemonic, "MOV_OUT") == 0)
		return MOV_OUT;
	else if (strcmp(instruction_mnemonic, "SUM") == 0)
		return SUM;
	else if (strcmp(instruction_mnemonic, "SUB") == 0)
		return SUB;
	else if (strcmp(instruction_mnemonic, "JNZ") == 0)
		return JNZ;
	else if (strcmp(instruction_mnemonic, "COPY_MEM") == 0)
		return COPY_MEM;
    else if (strcmp(instruction_mnemonic, "MUTEX_CREATE") == 0)
		return INS_MUTEX_CREATE;
    else if (strcmp(instruction_mnemonic, "MUTEX_LOCK") == 0)
		return INS_MUTEX_LOCK;
    else if (strcmp(instruction_mnemonic, "MUTEX_UNLOCK") == 0)
		return INS_MUTEX_UNLOCK;
    else if (strcmp(instruction_mnemonic, "MEM_ALLOC") == 0)
		return INS_MEM_ALLOC;
    else if (strcmp(instruction_mnemonic, "MEM_FREE") == 0)
		return INS_MEM_FREE;
    else if (strcmp(instruction_mnemonic, "SLEEP") == 0)
		return INS_SLEEP;
    else if (strcmp(instruction_mnemonic, "STDOUT") == 0)
		return INS_STDOUT;
    else if (strcmp(instruction_mnemonic, "STDIN") == 0)
		return INS_STDIN;
    else if (strcmp(instruction_mnemonic, "INIT_PROC") == 0)
		return INS_INIT_PROC;
    else if (strcmp(instruction_mnemonic, "EXIT") == 0)
        return INS_EXIT;
	else
		return UNKNOWN;
}


t_register_descriptor get_register_descriptor(t_cpu_context *context, const char *register_name) {
    t_register_descriptor descriptor = { NULL, 0 };

    if (strcasecmp(register_name, "ax") == 0) {
        descriptor.field_address = &context->ax;
        descriptor.field_size = sizeof(context->ax);
    } else if (strcasecmp(register_name, "bx") == 0) {
        descriptor.field_address = &context->bx;
        descriptor.field_size = sizeof(context->bx);
    } else if (strcasecmp(register_name, "cx") == 0) {
        descriptor.field_address = &context->cx;
        descriptor.field_size = sizeof(context->cx);
    } else if (strcasecmp(register_name, "dx") == 0) {
        descriptor.field_address = &context->dx;
        descriptor.field_size = sizeof(context->dx);
    } else if (strcasecmp(register_name, "eax") == 0) {
        descriptor.field_address = &context->eax;
        descriptor.field_size = sizeof(context->eax);
    } else if (strcasecmp(register_name, "ebx") == 0) {
        descriptor.field_address = &context->ebx;
        descriptor.field_size = sizeof(context->ebx);
    } else if (strcasecmp(register_name, "ecx") == 0) {
        descriptor.field_address = &context->ecx;
        descriptor.field_size = sizeof(context->ecx);
    } else if (strcasecmp(register_name, "edx") == 0) {
        descriptor.field_address = &context->edx;
        descriptor.field_size = sizeof(context->edx);
    } else if (strcasecmp(register_name, "si") == 0) {
        descriptor.field_address = &context->si;
        descriptor.field_size = sizeof(context->si);
    } else if (strcasecmp(register_name, "di") == 0) {
        descriptor.field_address = &context->di;
        descriptor.field_size = sizeof(context->di);
    }

    return descriptor;
}

uint32_t read_register_value(t_cpu_context *context, const char *register_name) {
    t_register_descriptor descriptor = get_register_descriptor(context, register_name);
    if (descriptor.field_address == NULL) {
        log_error(logger, "Invalid register: %s", register_name);
        return 0;
    }

    if (descriptor.field_size == sizeof(uint8_t)) {
        return *(uint8_t *)descriptor.field_address;
    }
    return *(uint32_t *)descriptor.field_address;
}

bool write_register_value(t_cpu_context *context, const char *register_name, uint32_t value) {
    t_register_descriptor descriptor = get_register_descriptor(context, register_name);
    if (descriptor.field_address == NULL) {
        log_error(logger, "Invalid register: %s", register_name);
        return false;
    }

    if (descriptor.field_size == sizeof(uint8_t)) {
        *(uint8_t *)descriptor.field_address = (uint8_t)value;
    } else {
        *(uint32_t *)descriptor.field_address = value;
    }
    return true;
}

void execute_instruction(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table, bool *hasJumped) {
	t_instruction_type instruction = instruction_to_type(decoded_instruction[0]);
	
	switch (instruction) {
		case NOOP: {
			log_debug(logger, "NOOP ejecutado");
			break;
		}

		case SET: {
			instruction_set(decoded_instruction, context);
			log_debug(logger, "SET ejecutado");
			break;
		}

        case MOV_IN: {
            instruction_mov_in(decoded_instruction, context, pid, segment_table);
            log_debug(logger, "MOV_IN ejecutado");
            break;
        }

        case MOV_OUT: { 
            instruction_mov_out(decoded_instruction, context, pid, segment_table);
            log_debug(logger, "MOV_OUT ejecutado");
            break;
        }
		
		case SUM: {
			instruction_sum(decoded_instruction, context);
			log_debug(logger, "SUM ejecutado");
			break;
		}

        case SUB: {
			instruction_sub(decoded_instruction, context);
			log_debug(logger, "SUB ejecutado");
			break;
		}

        case JNZ: {
			instruction_jnz(decoded_instruction, context, hasJumped);
			log_debug(logger, "JNZ ejecutado");
			break;
		}

        case COPY_MEM: {
            instruction_copy_mem(decoded_instruction, context, pid, segment_table);
            break;
        }

        case INS_MUTEX_CREATE: {
            instruction_mutex_create(decoded_instruction, context, pid, hasJumped);
            log_debug(logger, "MUTEX_CREATE ejecutado");
            break;
        }

        case INS_MUTEX_LOCK: {
            instruction_mutex_lock(decoded_instruction, context, pid, hasJumped);
            log_debug(logger, "MUTEX_LOCK ejecutado");
            break;
        }

        case INS_MUTEX_UNLOCK: {
            instruction_mutex_unlock(decoded_instruction, context, pid, hasJumped);
            log_debug(logger, "MUTEX_UNLOCK ejecutado");
            break;
        }

        case INS_MEM_ALLOC: {
            instruction_mem_alloc(decoded_instruction, context, pid, hasJumped);
            log_debug(logger, "MEM_ALLOC ejecutado");
            break;
        }

        case INS_MEM_FREE: {
            instruction_mem_free(decoded_instruction, context, pid, hasJumped);
            log_debug(logger, "MEM_FREE ejecutado");
            break;
        }

        case INS_SLEEP: {
            instruction_sleep(decoded_instruction, context, pid, hasJumped);
            log_debug(logger, "SLEEP ejecutado");
            break;
        }

        case INS_STDOUT: {
            instruction_stdout(decoded_instruction, context, pid, segment_table, hasJumped);
            log_debug(logger, "STDOUT ejecutado");
            break;
        }

        case INS_STDIN: {
            instruction_stdin(decoded_instruction, context, pid, segment_table, hasJumped);
            log_debug(logger, "STDIN ejecutado");
            break;
        }

		case INS_INIT_PROC: {
			instruction_init_proc(decoded_instruction, context, pid);
			log_debug(logger, "INIT_PROC ejecutado");
			break;
		}

		case INS_EXIT: {
			instruction_exit(decoded_instruction, context, pid);
			log_debug(logger, "EXIT ejecutado");
			break;
		}

        case UNKNOWN: {
            log_warning(logger, "Instruccion desconocida: %s", decoded_instruction[0]);
            break;
        }
	}
}

void instruction_set(char **decoded_instruction, t_cpu_context *context){
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[1]);
        return;
    }

    uint32_t value = atoi(decoded_instruction[2]);
    if (!write_register_value(context, decoded_instruction[1], value)) {
        log_error(logger, "Failed to set register: %s", decoded_instruction[1]);
    }
}

void instruction_mov_in(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table) {
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid destination register: %s", decoded_instruction[1]);
        return;
    }

    t_register_descriptor descriptor = get_register_descriptor(context, decoded_instruction[1]);
    uint32_t physical_address = mmu_translate(context->si, descriptor.field_size, segment_table, pid);
    
    if (should_exit) return;

    void *data = memory_read(physical_address, descriptor.field_size);
    if (data == NULL) {
        log_error(logger, "MOV_IN: error leyendo dirección física %u", physical_address);
        return;
    }

    if (descriptor.field_size == sizeof(uint8_t)) {
        *(uint8_t *)descriptor.field_address = *(uint8_t *)data;
    } else {
        *(uint32_t *)descriptor.field_address = *(uint32_t *)data;
    }

    free(data);
    log_debug(logger, "MOV_IN - SI=%u - dir. físico=%u - %s=%u", context->si, physical_address, decoded_instruction[1], read_register_value(context, decoded_instruction[1]));
}

void instruction_mov_out(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table) {
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "MOV_OUT: registro inválido: %s", decoded_instruction[1]);
        return;
    }

    t_register_descriptor descriptor = get_register_descriptor(context, decoded_instruction[1]);
    uint32_t physical_address = mmu_translate(context->di, descriptor.field_size, segment_table, pid);
    if (should_exit) return;

    uint32_t value = read_register_value(context, decoded_instruction[1]);
    bool ok = memory_write(physical_address, &value, descriptor.field_size);
    if (!ok) {
        log_error(logger, "MOV_OUT: error escribiendo dirección física %u", physical_address);
        return;
    }

    log_debug(logger, "MOV_OUT - DI=%u - dir. físico=%u - %s=%u", context->di, physical_address, decoded_instruction[1], value);
}

void instruction_sum(char **decoded_instruction, t_cpu_context *context){
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid destination register: %s", decoded_instruction[1]);
        return;
    }
    if (!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid source register: %s", decoded_instruction[2]);
        return;
    }

    uint32_t left_value = read_register_value(context, decoded_instruction[1]);
    uint32_t right_value = read_register_value(context, decoded_instruction[2]);

    if (!write_register_value(context, decoded_instruction[1], left_value + right_value)) {
        log_error(logger, "Failed to write SUM result to register: %s", decoded_instruction[1]);
    }
}

void instruction_sub(char **decoded_instruction, t_cpu_context *context){
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid destination register: %s", decoded_instruction[1]);
        return;
    }
    if (!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid source register: %s", decoded_instruction[2]);
        return;
    }

    uint32_t left_value = read_register_value(context, decoded_instruction[1]);
    uint32_t right_value = read_register_value(context, decoded_instruction[2]);

    if (!write_register_value(context, decoded_instruction[1], left_value - right_value)) {
        log_error(logger, "Failed to write SUB result to register: %s", decoded_instruction[1]);
    }
}

void instruction_jnz(char **decoded_instruction, t_cpu_context *context, bool *hasJumped){
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[1]);
        return;
    }
    if (atoi(decoded_instruction[2]) <= 0) {
        log_error(logger, "Invalid jump address: %s", decoded_instruction[2]);
        return;
    }

    if (read_register_value(context, decoded_instruction[1]) != 0) {
        context->pc = atoi(decoded_instruction[2]);
        *hasJumped = true;
    }
}

bool check_if_register(char *operand) {
    if (strcasecmp(operand, "ax") == 0 || strcasecmp(operand, "bx") == 0 || strcasecmp(operand, "cx") == 0 || strcasecmp(operand, "dx") == 0 ||
        strcasecmp(operand, "eax") == 0 || strcasecmp(operand, "ebx") == 0 || strcasecmp(operand, "ecx") == 0 || strcasecmp(operand, "edx") == 0 ||
        strcasecmp(operand, "si") == 0 || strcasecmp(operand, "di") == 0) {
        return true;
    }
    return false;
}

void instruction_copy_mem(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table) {
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "COPY_MEM: registro de tamaño inválido: %s", decoded_instruction[1]);
        return;
    }

    uint32_t size = read_register_value(context, decoded_instruction[1]);

    uint32_t physical_src = mmu_translate(context->si, size, segment_table, pid);
    if (should_exit) return;

    uint32_t physical_dst = mmu_translate(context->di, size, segment_table, pid);
    if (should_exit) return;

    void *data = memory_read(physical_src, size);
    if (data == NULL) {
        log_error(logger, "COPY_MEM: error leyendo dirección física %u", physical_src);
        return;
    }

    bool ok = memory_write(physical_dst, data, size);
    free(data);
    if (!ok) {
        log_error(logger, "COPY_MEM: error escribiendo dirección física %u", physical_dst);
        return;
    }
    log_debug(logger, "COPY_MEM - SI=%u (dir. físico=%u) - DI=%u (dir. físico=%u) - size=%u", context->si, physical_src, context->di, physical_dst, size);
}

void *process_execution_handler(void *args) {
	t_process_execution_args *exec_args = (t_process_execution_args *)args;
	uint32_t exec_pid = exec_args->pid;
	t_cpu_context *exec_context = exec_args->context;
    t_list *segment_table = exec_args->segment_table;

    // Mark that this PID has an active execution thread
    is_executing = true;
    instructions_cicle(exec_context, exec_pid, segment_table);
    // Execution finished
    is_executing = false;
	
	free(exec_context);
	free(exec_args);
	
	return NULL;
}

// Syscall instructions implementations

void instruction_mutex_create(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped) {
    // Prepare package for MUTEX_CREATE
    t_package *pkg = package_create();
    pkg->op_code = MUTEX_CREATE;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, decoded_instruction[1], strlen(decoded_instruction[1]) + 1);

    // Advance PC so context sent reflects next instruction and avoid re-execution
    pthread_mutex_lock(&process_control_mutex);
    context->pc++;
    *hasJumped = true;
    stop_reason = MUTEX_REQUEST;
    should_stop = true;
    // Send context to Kernel Memory so KM stores the updated PC, then notify Scheduler
    context_send(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
    pthread_mutex_unlock(&process_control_mutex);

    // Send package to scheduler
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_mutex_lock(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped) {
    // Prepare package for MUTEX_LOCK
    t_package *pkg = package_create();
    pkg->op_code = MUTEX_LOCK;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, decoded_instruction[1], strlen(decoded_instruction[1]) + 1);

    // Advance PC so context sent reflects next instruction and avoid re-execution
    pthread_mutex_lock(&process_control_mutex);
    context->pc++;
    *hasJumped = true;
    stop_reason = MUTEX_REQUEST;
    should_stop = true;
    // Send context to Kernel Memory so KM stores the updated PC, then notify Scheduler
    context_send(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
    pthread_mutex_unlock(&process_control_mutex);

    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_mutex_unlock(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped) {
    // Prepare package for MUTEX_UNLOCK
    t_package *pkg = package_create();
    pkg->op_code = MUTEX_UNLOCK;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, decoded_instruction[1], strlen(decoded_instruction[1]) + 1);

    // Advance PC so context sent reflects next instruction and avoid re-execution
    pthread_mutex_lock(&process_control_mutex);
    context->pc++;
    *hasJumped = true;
    stop_reason = MUTEX_REQUEST;
    should_stop = true;
    // Send context to Kernel Memory so KM stores the updated PC, then notify Scheduler
    context_send(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
    pthread_mutex_unlock(&process_control_mutex);

    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_mem_alloc(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped) {
    uint32_t segment_id = atoi(decoded_instruction[1]);
    uint32_t size = atoi(decoded_instruction[2]);
    // Prepare package for MEM_ALLOC
    t_package *pkg = package_create();
    pkg->op_code = MEM_ALLOC;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &segment_id, sizeof(uint32_t));
    package_add(pkg, &size, sizeof(uint32_t));

    // Advance PC so context sent reflects next instruction and avoid re-execution
    pthread_mutex_lock(&process_control_mutex);
    context->pc++;
    *hasJumped = true;
    stop_reason = MEMORY_REQUEST;
    should_stop = true;
    // Send context to Kernel Memory so KM stores the updated PC, then notify Scheduler
    context_send(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
    pthread_mutex_unlock(&process_control_mutex);

    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_mem_free(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped) {
    uint32_t address = atoi(decoded_instruction[1]);
    // Prepare package for MEM_FREE
    t_package *pkg = package_create();
    pkg->op_code = MEM_FREE;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &address, sizeof(uint32_t));

    // Advance PC so context sent reflects next instruction and avoid re-execution
    pthread_mutex_lock(&process_control_mutex);
    context->pc++;
    *hasJumped = true;
    stop_reason = MEMORY_REQUEST;
    should_stop = true;
    // Send context to Kernel Memory so KM stores the updated PC, then notify Scheduler
    context_send(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
    pthread_mutex_unlock(&process_control_mutex);

    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_sleep(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped) {
    uint32_t time = atoi(decoded_instruction[1]);
    // Prepare package for SLEEP
    t_package *pkg = package_create();
    pkg->op_code = SLEEP;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &time, sizeof(uint32_t));

    // Advance PC so context sent reflects next instruction and avoid re-execution
    pthread_mutex_lock(&process_control_mutex);
    context->pc++;
    *hasJumped = true;
    stop_reason = IO_REQUEST;
    should_stop = true; // Deberia parar por generar interrupcion de IO
    // Send context to Kernel Memory so KM stores the updated PC, then notify Scheduler
    context_send(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
    pthread_mutex_unlock(&process_control_mutex);

    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_stdout(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table, bool *hasJumped) {
    // Send STDOUT operation to kernel scheduler with logical address and size registers
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid STDOUT register operand: %s", decoded_instruction[1]);
        return;
    }
    if(!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid STDOUT register operand: %s", decoded_instruction[2]);
        return;
    }

    uint32_t size = read_register_value(context, decoded_instruction[2]);
    uint32_t physicall_address = mmu_translate(read_register_value(context, decoded_instruction[1]), size, segment_table, pid);
    if (should_exit) return;

    t_package *pkg = package_create();
    pkg->op_code = STDOUT;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &physicall_address, sizeof(uint32_t));
    package_add(pkg, &size, sizeof(uint32_t));

    // Advance PC so context sent reflects next instruction and avoid re-execution
    pthread_mutex_lock(&process_control_mutex);
    context->pc++;
    *hasJumped = true;
    stop_reason = IO_REQUEST;
    should_stop = true; // Deberia parar por generar interrupcion de IO
    // Send context to Kernel Memory so KM stores the updated PC, then notify Scheduler
    context_send(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
    pthread_mutex_unlock(&process_control_mutex);

    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_stdin(char **decoded_instruction, t_cpu_context *context, uint32_t pid, t_list *segment_table, bool *hasJumped) {
    // Send STDIN operation to kernel scheduler with logical address and size registers
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid STDIN register operand: %s", decoded_instruction[1]);
        return;
    }
    if(!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid STDIN register operand: %s", decoded_instruction[2]);
        return;
    }

    uint32_t size = read_register_value(context, decoded_instruction[2]);
    uint32_t physicall_address = mmu_translate(read_register_value(context, decoded_instruction[1]), size, segment_table, pid);
    if (should_exit) return;

    t_package *pkg = package_create();
    pkg->op_code = STDIN;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &physicall_address, sizeof(uint32_t));
    package_add(pkg, &size, sizeof(uint32_t));

    // Send context to Kernel Memory first to avoid race, then notify Scheduler
    pthread_mutex_lock(&process_control_mutex);
    context_send(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
    // Advance PC so context sent reflects next instruction and avoid re-execution
    context->pc++;
    *hasJumped = true;
    stop_reason = IO_REQUEST;
    should_stop = true; // Deberia parar por generar interrupcion de IO
    pthread_mutex_unlock(&process_control_mutex);

    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_init_proc(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    uint32_t priority = atoi(decoded_instruction[2]);
    // Send PROCESS_CREATE operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_CREATE;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &priority, sizeof(uint32_t));
    package_add(pkg, decoded_instruction[1], strlen(decoded_instruction[1]) + 1);
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_exit(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    // Send PROCESS_END operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_END;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
    pthread_mutex_lock(&process_control_mutex);
    should_exit = true;
    pthread_mutex_unlock(&process_control_mutex);
}

uint32_t mmu_translate(uint32_t logical_address, uint32_t size, t_list *segment_table, uint32_t pid) {
    uint32_t num_segment = logical_address / segment_max_size;
    uint32_t seg_offset = logical_address % segment_max_size;

    if (num_segment >= (uint32_t)list_size(segment_table)) { // Accessing non-existing segment
        log_error(logger, "PID: %d - SEGMENTATION_FAULT", pid);
        t_package *pkg = package_create();
        pkg->op_code = SEGMENTATION_FAULT;
        package_add(pkg, &pid, sizeof(uint32_t));
        package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
        package_delete(pkg);
        pthread_mutex_lock(&process_control_mutex);
        should_exit = true;
        pthread_mutex_unlock(&process_control_mutex);
        return 0;
    }

    t_segment *segment = list_get(segment_table, num_segment);

    if (seg_offset + size > segment->size) { // Access beyond segment limit
        log_error(logger, "PID: %d - Acceso fuera de segmento (offset=%d, size=%d, seg_size=%d) - SEGMENTATION_FAULT", pid, seg_offset, size, segment->size);
        t_package *pkg = package_create();
        pkg->op_code = SEGMENTATION_FAULT;
        package_add(pkg, &pid, sizeof(uint32_t));
        package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
        package_delete(pkg);
        pthread_mutex_lock(&process_control_mutex);
        should_exit = true;
        pthread_mutex_unlock(&process_control_mutex);
        return 0;
    }

    uint32_t physical_address = segment->base + seg_offset;
    log_info(logger, "PID: %d - MMU - Dirección lógica %d → física %d (segmento=%d, desplazamiento=%d)", pid, logical_address, physical_address, num_segment, seg_offset);

    return physical_address;
}

void send_process_interrupted(uint32_t pid, t_interrupt_reason reason) {
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_INTERRUPTED;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &reason, sizeof(t_interrupt_reason));
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
    log_debug(logger, "Se ha informado a Kernel Scheduler de interrupcion para PID %d (razon=%s)", pid, interrupt_reason_to_string(reason));
}

// =================================================================================
// POR AHORA LAS DEFINO EN CPU, DEBERIAN IR EN UN UTILS DE MEMORIA COMPARTIDO CON KM
// =================================================================================

t_memory_stick_info *get_memory_stick_by_address(uint32_t physical_address, uint32_t *local_offset) {
    uint32_t cursor = 0;
    pthread_mutex_lock(&memory_stick_list_mutex);
    for (int i = 0; i < list_size(list_memory_stick); i++) {
        t_memory_stick_info *ms = list_get(list_memory_stick, i);
        if (physical_address < cursor + ms->size) {
            *local_offset = physical_address - cursor;
            pthread_mutex_unlock(&memory_stick_list_mutex);
            return ms;
        }
        cursor += ms->size;
    }
    pthread_mutex_unlock(&memory_stick_list_mutex);
    return NULL;
}

void *memory_read(uint32_t physical_address, uint32_t size) {
    void *result = malloc(size);
    if (result == NULL) return NULL;

    uint32_t bytes_done = 0;

    while (bytes_done < size) {
        uint32_t local_offset;
        t_memory_stick_info *ms = get_memory_stick_by_address(physical_address + bytes_done, &local_offset);

        if (ms == NULL) {
            log_error(logger, "## memory_read: dirección física %u fuera de rango", physical_address + bytes_done);
            free(result);
            return NULL;
        }

        uint32_t available_in_stick = ms->size - local_offset;
        uint32_t remaining = size - bytes_done;
        uint32_t chunk = (remaining < available_in_stick) ? remaining : available_in_stick;

        t_package *pkg = package_create();
        pkg->op_code = MS_READ;
        package_add(pkg, &local_offset, sizeof(uint32_t));
        package_add(pkg, &chunk, sizeof(uint32_t));
        package_send(pkg, ms->fd, &ms->network_mutex);
        package_delete(pkg);

        sem_wait(&ms->response_sem);
        pthread_mutex_lock(&ms->mutex);
        void *chunk_data = ms->last_read_buffer;
        ms->last_read_buffer = NULL;
        pthread_mutex_unlock(&ms->mutex);

        char *data_str = bytes_to_safe_string(chunk_data, chunk);
        log_debug(logger, "memory_read: Dato leido: %s", data_str);
        free(data_str);

        if (chunk_data == NULL) {
            log_error(logger, "## memory_read: chunk NULL en MS id=%d", ms->id);
            free(result);
            return NULL;
        }

        memcpy(result + bytes_done, chunk_data, chunk);
        free(chunk_data);
        bytes_done += chunk;
    }

    return result;
}

bool memory_write(uint32_t physical_address, void *data, uint32_t size) {
    uint32_t bytes_done = 0;

    while (bytes_done < size) {
        uint32_t local_offset;
        t_memory_stick_info *ms = get_memory_stick_by_address(physical_address + bytes_done, &local_offset);

        if (ms == NULL) {
            log_error(logger, "## memory_write: dirección física %u fuera de rango", physical_address + bytes_done);
            return false;
        }

        uint32_t available_in_stick = ms->size - local_offset;
        uint32_t remaining = size - bytes_done;
        uint32_t chunk = (remaining < available_in_stick) ? remaining : available_in_stick;

        t_package *pkg = package_create();
        pkg->op_code = MS_WRITE;
        package_add(pkg, &local_offset, sizeof(uint32_t));
        package_add(pkg, &chunk, sizeof(uint32_t));
        package_add(pkg, data + bytes_done, chunk);
        package_send(pkg, ms->fd, &ms->network_mutex);
        package_delete(pkg);

        sem_wait(&ms->response_sem);
        pthread_mutex_lock(&ms->mutex);
        bool chunk_ok = (ms->last_op_result == MS_WRITE_RESPONSE);
        ms->last_op_result = -1;
        pthread_mutex_unlock(&ms->mutex);

        if (!chunk_ok) {
            log_error(logger, "## memory_write: falló escritura en MS id=%d offset=%u chunk=%u", ms->id, local_offset, chunk);
            return false;
        }

        bytes_done += chunk;
    }

    return true;
}
