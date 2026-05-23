#include<manage_instructions.h>

static void send_package_to_kernel_scheduler(t_package *pkg) {
    pthread_mutex_lock(&kernel_scheduler_write_mutex);
    package_send(pkg, kernel_scheduler_fd);
    pthread_mutex_unlock(&kernel_scheduler_write_mutex);
}

static void send_package_to_kernel_memory(t_package *pkg) {
    pthread_mutex_lock(&kernel_memory_write_mutex);
    package_send(pkg, kernel_memory_fd);
    pthread_mutex_unlock(&kernel_memory_write_mutex);
}

void instructions_cicle(t_cpu_context *context, uint32_t pid) {
    bool hasJumped = false;
	while (1)
	{
		t_package *pkg = package_create();
		pkg->op_code = INSTRUCTION_FETCH;
		package_add(pkg, &pid, sizeof(uint32_t));
		send_package_to_kernel_memory(pkg);
		package_delete(pkg);
		log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid, context->pc);
		
        pthread_mutex_lock(&kernel_memory_read_mutex);
        char *instruction = message_receive(logger, kernel_memory_fd); // a chequear si estan bien los parametros
		pthread_mutex_unlock(&kernel_memory_read_mutex);
        
        if (instruction == NULL) {
			log_error(logger, "Failed to receive instruction from Kernel Memory");
			context->pc++;
			break;
		}
		log_info(logger, "Received instruction from Kernel Memory: %s", instruction);
		char **decoded_instruction = decode_instruction(instruction);
		execute_instruction(decoded_instruction, context, pid, &hasJumped);
		log_info(logger, "## PID: %d - Ejecutando: %s", pid, decoded_instruction[0]);
		free(instruction);
		string_array_destroy(decoded_instruction);
		
        if (!hasJumped) {
            context->pc++;
        } else {
            hasJumped = false;
        }

        pthread_mutex_lock(&interrupt_mutex);
        bool interrupt = interruptPending;
        pthread_mutex_unlock(&interrupt_mutex);

		if(interrupt) {
            log_info(logger, "## Interrupción recibida");
			log_info(logger, "Interrupt pending for PID %d, sending context to Kernel Memory", pid);
			t_package *pkg = package_create();
			pkg->op_code = CONTEXT_TRANSFER;
			package_add(pkg, &pid, sizeof(uint32_t));
			pthread_mutex_lock(&kernel_memory_write_mutex);
            package_send(pkg, kernel_memory_fd);
            context_send(context, kernel_memory_fd);
            pthread_mutex_unlock(&kernel_memory_write_mutex);
            package_delete(pkg);
            log_info(logger, "Context saved to Kernel Memory");

            uint32_t reason = 1; // Tengo que hacer generica
            t_package *pkg = package_create();
            pkg->op_code = PROCESS_INTERRUPTED;
            package_add(pkg, &pid, sizeof(uint32_t));
            package_add(pkg, &reason, sizeof(uint32_t));
            send_package_to_kernel_scheduler(pkg);
            package_delete(pkg);
            log_info(logger, "Notified Kernel Scheduler about PID %d interruption (reason=%d)", pid, reason);
            

            pthread_mutex_lock(&interrupt_mutex);
			interruptPending = 0;
            pthread_mutex_unlock(&interrupt_mutex);
			break;
		}
		log_info(logger, "No interrupt pending for PID %d, continuing execution", pid);
	}
}

char **decode_instruction(char *content) {
	char **decoded_instruction = string_split(content, " ");
	return decoded_instruction;
}

t_instruction_type instruction_to_type(char *instruction_mnemonic) {
	if (strcmp(instruction_mnemonic, "NO_OP") == 0)
		return NO_OP;
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

    if (strcmp(register_name, "ax") == 0) {
        descriptor.field_address = &context->ax;
        descriptor.field_size = sizeof(context->ax);
    } else if (strcmp(register_name, "bx") == 0) {
        descriptor.field_address = &context->bx;
        descriptor.field_size = sizeof(context->bx);
    } else if (strcmp(register_name, "cx") == 0) {
        descriptor.field_address = &context->cx;
        descriptor.field_size = sizeof(context->cx);
    } else if (strcmp(register_name, "dx") == 0) {
        descriptor.field_address = &context->dx;
        descriptor.field_size = sizeof(context->dx);
    } else if (strcmp(register_name, "eax") == 0) {
        descriptor.field_address = &context->eax;
        descriptor.field_size = sizeof(context->eax);
    } else if (strcmp(register_name, "ebx") == 0) {
        descriptor.field_address = &context->ebx;
        descriptor.field_size = sizeof(context->ebx);
    } else if (strcmp(register_name, "ecx") == 0) {
        descriptor.field_address = &context->ecx;
        descriptor.field_size = sizeof(context->ecx);
    } else if (strcmp(register_name, "edx") == 0) {
        descriptor.field_address = &context->edx;
        descriptor.field_size = sizeof(context->edx);
    } else if (strcmp(register_name, "si") == 0) {
        descriptor.field_address = &context->si;
        descriptor.field_size = sizeof(context->si);
    } else if (strcmp(register_name, "di") == 0) {
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

void execute_instruction(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped) {
	t_instruction_type instruction = instruction_to_type(decoded_instruction[0]);
	
	switch (instruction) {
		case NO_OP: {
			log_info(logger, "NO_OP executed");
			break;
		}

		case SET: {
			instruction_set(decoded_instruction, context);
			log_info(logger, "SET executed");
			break;
		}

		case MOV_IN:{
            //Llamar a mmu dir_logica → dir_fisica
            log_info(logger, "Memory operations not yet implemented. OK for now.");
            break;
        }

		case MOV_OUT: {
            //Llamar a mmu dir_logica → dir_fisica
			log_info(logger, "Memory operations not yet implemented. OK for now.");
			break;
        }
		
		case SUM: {
			instruction_sum(decoded_instruction, context);
			log_info(logger, "SUM executed");
			break;
		}

        case SUB: {
			instruction_sub(decoded_instruction, context);
			log_info(logger, "SUB executed");
			break;
		}

        case JNZ: {
			instruction_jnz(decoded_instruction, context, hasJumped);
			log_info(logger, "JNZ executed");
			break;
		}

        case COPY_MEM: {
           //Llamar a mmu dir_logica → dir_fisica
			log_info(logger, "Memory operations not yet implemented. OK for now.");
			break;
        }

		case INS_MUTEX_CREATE: {
			instruction_mutex_create(decoded_instruction, context);
			log_info(logger, "MUTEX_CREATE executed");
			break;
		}

		case INS_MUTEX_LOCK: {
			instruction_mutex_lock(decoded_instruction, context);
			log_info(logger, "MUTEX_LOCK executed");
			break;
		}

		case INS_MUTEX_UNLOCK: {
			instruction_mutex_unlock(decoded_instruction, context);
			log_info(logger, "MUTEX_UNLOCK executed");
			break;
		}

		case INS_MEM_ALLOC: {
			instruction_mem_alloc(decoded_instruction, context, pid);
			log_info(logger, "MEM_ALLOC executed");
			break;
		}

		case INS_MEM_FREE: {
			instruction_mem_free(decoded_instruction, context, pid);
			log_info(logger, "MEM_FREE executed");
			break;
		}

		case INS_SLEEP: {
			instruction_sleep(decoded_instruction, context, pid);
			log_info(logger, "SLEEP executed");
			break;
		}

		case INS_STDOUT: {
			instruction_stdout(decoded_instruction, context, pid);
			log_info(logger, "STDOUT executed");
			break;
		}

		case INS_STDIN: {
			instruction_stdin(decoded_instruction, context, pid);
			log_info(logger, "STDIN executed");
			break;
		}

		case INS_INIT_PROC: {
			instruction_init_proc(decoded_instruction, context, pid);
			log_info(logger, "INIT_PROC executed");
			break;
		}

		case INS_EXIT: {
			instruction_exit(decoded_instruction, context, pid);
			log_info(logger, "EXIT executed");
			break;
		}

        case UNKNOWN: {
            log_warning(logger, "Unknown instruction: %s", decoded_instruction[0]);
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
	if (strcmp(operand, "ax") == 0 || strcmp(operand, "bx") == 0 || strcmp(operand, "cx") == 0 || strcmp(operand, "dx") == 0 ||
		strcmp(operand, "eax") == 0 || strcmp(operand, "ebx") == 0 || strcmp(operand, "ecx") == 0 || strcmp(operand, "edx") == 0 ||
		strcmp(operand, "si") == 0 || strcmp(operand, "di") == 0) {
		return true;
	}
	return false;
}

void *process_execution_handler(void *args) {
	t_process_execution_args *exec_args = (t_process_execution_args *)args;
	uint32_t exec_pid = exec_args->pid;
	t_cpu_context *exec_context = exec_args->context;
	
	instructions_cicle(exec_context, exec_pid);
	
	free(exec_context);
	free(exec_args);
	
	return NULL;
}

// Syscall instruction implementations

void instruction_mutex_create(char **decoded_instruction, t_cpu_context *context) {
    // Send MUTEX_CREATE operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = MUTEX_CREATE;
    package_add(pkg, decoded_instruction[1], strlen(decoded_instruction[1]) + 1);
    send_package_to_kernel_scheduler(pkg);
    package_delete(pkg);
}

void instruction_mutex_lock(char **decoded_instruction, t_cpu_context *context) {
    // Send MUTEX_LOCK operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = MUTEX_LOCK;
    package_add(pkg, decoded_instruction[1], strlen(decoded_instruction[1]) + 1);
    send_package_to_kernel_scheduler(pkg);
    package_delete(pkg);
}

void instruction_mutex_unlock(char **decoded_instruction, t_cpu_context *context) {
    // Send MUTEX_UNLOCK operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = MUTEX_UNLOCK;
    package_add(pkg, decoded_instruction[1], strlen(decoded_instruction[1]) + 1);
    send_package_to_kernel_scheduler(pkg);
    package_delete(pkg);
}

void instruction_mem_alloc(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    uint32_t size = atoi(decoded_instruction[1]);
    // Send MEM_ALLOC operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = MEM_ALLOC;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &size, sizeof(uint32_t));
    send_package_to_kernel_scheduler(pkg);
    package_delete(pkg);
}

void instruction_mem_free(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    uint32_t address = atoi(decoded_instruction[1]);
    // Send MEM_FREE operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = MEM_FREE;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &address, sizeof(uint32_t));
    send_package_to_kernel_scheduler(pkg);
    package_delete(pkg);
}

void instruction_sleep(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    uint32_t time = atoi(decoded_instruction[1]);
    // Send SLEEP operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = SLEEP;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &time, sizeof(uint32_t));
    send_package_to_kernel_scheduler(pkg);
    package_delete(pkg);
}

void instruction_stdout(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    // Send STDOUT operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = STDOUT;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, decoded_instruction[1], strlen(decoded_instruction[1]) + 1);
    send_package_to_kernel_scheduler(pkg);
    package_delete(pkg);
}

void instruction_stdin(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    // Send STDIN operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = STDIN;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, decoded_instruction[1], strlen(decoded_instruction[1]) + 1);
    send_package_to_kernel_scheduler(pkg);
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
    send_package_to_kernel_scheduler(pkg);
    package_delete(pkg);
}

void instruction_exit(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    // Send PROCESS_END operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_END;
    package_add(pkg, &pid, sizeof(uint32_t));
    send_package_to_kernel_scheduler(pkg);
    package_delete(pkg);
}

/*  Idea de traduccion con MMU, falta implementar las tablas, tamaños, etc
int32_t mmu_translate(t_cpu_context *context, uint32_t dir_logica, uint32_t size) {
    uint32_t num_segmento   = dir_logica / SEGMENT_MAX_SIZE;
    uint32_t desplazamiento = dir_logica % SEGMENT_MAX_SIZE;

    // Verificar que el segmento existe en la tabla
    if (num_segmento >= context->segment_table_size) {
        log_error(logger, "Segfault: segmento %d no existe", num_segmento);
        return -1;
    }

    t_segment seg = context->segment_table[num_segmento];

    // Verificar que no se sale del límite del segmento
    if (desplazamiento + size > seg.limit) {
        log_error(logger, "Segfault: acceso fuera del segmento");
        return -1;
    }

    return seg.base + desplazamiento;
}
*/
