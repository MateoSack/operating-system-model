#include<manage_instructions.h>

bool should_exit = false;
bool shoudld_stop = false;

void instructions_cicle(t_cpu_context *context, uint32_t pid) {
    bool hasJumped = false;
	while (1)
	{
        pthread_mutex_lock(&process_control_mutex);
        bool exit_flag = should_exit;
        if(exit_flag) {
            log_info(logger, "Finalizando proceso PID %d", pid);
            should_exit = false;
            pthread_mutex_unlock(&process_control_mutex);
            sem_post(&sem_eviction_ready);
        } else {
            pthread_mutex_unlock(&process_control_mutex);
        }

        pthread_mutex_unlock(&process_control_mutex);
        if(exit_flag) break;
        
        pthread_mutex_lock(&process_control_mutex);
        bool stop_flag = shoudld_stop;
        if(stop_flag) {
            log_info(logger, "Deteniendo proceso PID %d", pid);
            shoudld_stop = false;
            sem_post(&sem_eviction_ready);
        }
        pthread_mutex_unlock(&process_control_mutex);
        if(stop_flag) break;

		t_package *pkg = package_create();
		pkg->op_code = INSTRUCTION_FETCH;
		package_add(pkg, &pid, sizeof(uint32_t));
		package_add(pkg, &context->pc, sizeof(uint32_t));
		
		pthread_mutex_lock(&kernel_memory_write_mutex); // Usar funcion general de send
		package_send(pkg, kernel_memory->fd, &kernel_memory->network_mutex);
		pthread_mutex_unlock(&kernel_memory_write_mutex);
		package_delete(pkg);
		
		log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid, context->pc);
		
		// Signal kernel_memory_thread that we're ready to receive instruction
		sem_post(&sem_instruction_fetch_ready);
		
		// Wait for kernel_memory_thread to deliver the instruction
		sem_wait(&sem_instruction_response_ready);
        
        log_debug(logger, "sem_instruction_response_ready signaled for PID %d", pid);

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
		execute_instruction(decoded_instruction, context, pid, &hasJumped);
		log_info(logger, "## PID: %d - Ejecutando: %s", pid, decoded_instruction[0]);
		free(instruction);
		string_array_destroy(decoded_instruction);

        pthread_mutex_lock(&process_control_mutex);
        bool stop_after_instr = shoudld_stop;
        if (stop_after_instr) {
            log_debug(logger, "Deteniendo proceso PID %d luego de instrucción especial", pid);
            shoudld_stop = false;
        }
        pthread_mutex_unlock(&process_control_mutex);

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

            pthread_mutex_lock(&kernel_memory_write_mutex);
            context_send_with_pid(context, pid, kernel_memory->fd, &kernel_memory->network_mutex);
            pthread_mutex_unlock(&kernel_memory_write_mutex);
            log_info(logger, "Context saved to Kernel Memory");

            t_interrupt_reason reason_local;
            pthread_mutex_lock(&interrupt_mutex);
            reason_local = interruptReason;
            pthread_mutex_unlock(&interrupt_mutex);

            t_package *pkg2 = package_create();
            pkg2->op_code = PROCESS_INTERRUPTED;
            package_add(pkg2, &pid, sizeof(uint32_t));
            package_add(pkg2, &reason_local, sizeof(t_interrupt_reason));
            pthread_mutex_lock(&kernel_scheduler_write_mutex);
            package_send(pkg2, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
            pthread_mutex_unlock(&kernel_scheduler_write_mutex);
            package_delete(pkg2);
            log_info(logger, "Notified Kernel Scheduler about PID %d interruption (reason=%s)", pid, interrupt_reason_to_string(reason_local));

            sem_post(&sem_eviction_ready);

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

void execute_instruction(char **decoded_instruction, t_cpu_context *context, uint32_t pid, bool *hasJumped) {
	t_instruction_type instruction = instruction_to_type(decoded_instruction[0]);
	
	switch (instruction) {
		case NOOP: {
			log_info(logger, "NOOP executed");
			break;
		}

		case SET: {
			instruction_set(decoded_instruction, context);
			log_info(logger, "SET executed");
			break;
		}

		case MOV_IN: { /*
            uint32_t logical_addr = read_register_value(context, "si");
            uint32_t size = sizeof(uint32_t);

            int32_t physical_addr = mmu_translate(context, logical_addr, size, SEGMENT_MAX_SIZE);
            if (physical_addr == -1) {
                send_segfault_to_scheduler(pid);
                return;
            }

            uint32_t value = 0;
            if (mmu_read(physical_addr, size, &value,
                        stick_size, memory_stick_fds, stick_count) != 0) return;

            write_register_value(context, decoded_instruction[1], value);
            log_info(logger, "MOV_IN ejecutado");
            */
            break;
        }

        case MOV_OUT: { /*
            uint32_t logical_addr = read_register_value(context, "di");
            uint32_t value        = read_register_value(context, decoded_instruction[1]);
            uint32_t size         = sizeof(uint32_t);

            int32_t physical_addr = mmu_translate(context, logical_addr, size, SEGMENT_MAX_SIZE);
            if (physical_addr == -1) {
                send_segfault_to_scheduler(pid);
                return;
            }

            mmu_write(physical_addr, size, &value,
                    stick_size, memory_stick_fds, stick_count);
            log_info(logger, "MOV_OUT ejecutado");
            */
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
        /*
            uint32_t src_logical  = read_register_value(context, "si");
            uint32_t dst_logical  = read_register_value(context, "di");
            uint32_t size         = read_register_value(context, decoded_instruction[1]);

            int32_t src_physical = mmu_translate(context, src_logical, size, SEGMENT_MAX_SIZE);
            int32_t dst_physical = mmu_translate(context, dst_logical, size, SEGMENT_MAX_SIZE);

            if (src_physical == -1 || dst_physical == -1) {
                send_segfault_to_scheduler(pid);
                return;
            }

            void *buffer = malloc(size);
            if (mmu_read(src_physical, size, buffer,
                        stick_size, memory_stick_fds, stick_count) != 0) {
                free(buffer);
                return;
            }
            mmu_write(dst_physical, size, buffer,
                    stick_size, memory_stick_fds, stick_count);
            free(buffer);
            log_info(logger, "COPY_MEM ejecutado");
            */
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
	if (strcasecmp(operand, "ax") == 0 || strcasecmp(operand, "bx") == 0 || strcasecmp(operand, "cx") == 0 || strcasecmp(operand, "dx") == 0 ||
		strcasecmp(operand, "eax") == 0 || strcasecmp(operand, "ebx") == 0 || strcasecmp(operand, "ecx") == 0 || strcasecmp(operand, "edx") == 0 ||
		strcasecmp(operand, "si") == 0 || strcasecmp(operand, "di") == 0) {
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
    message_send_with_op_code(decoded_instruction[1], MUTEX_CREATE, kernel_scheduler->fd);
}

void instruction_mutex_lock(char **decoded_instruction, t_cpu_context *context) {
    // Send MUTEX_LOCK operation to kernel scheduler
    message_send_with_op_code(decoded_instruction[1], MUTEX_LOCK, kernel_scheduler->fd);
}

void instruction_mutex_unlock(char **decoded_instruction, t_cpu_context *context) {
    // Send MUTEX_UNLOCK operation to kernel scheduler
    message_send_with_op_code(decoded_instruction[1], MUTEX_UNLOCK, kernel_scheduler->fd);
}

void instruction_mem_alloc(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    uint32_t size = atoi(decoded_instruction[1]);
    // Send MEM_ALLOC operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = MEM_ALLOC;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &size, sizeof(uint32_t));
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_mem_free(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    uint32_t address = atoi(decoded_instruction[1]);
    // Send MEM_FREE operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = MEM_FREE;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &address, sizeof(uint32_t));
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}

void instruction_sleep(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    uint32_t time = atoi(decoded_instruction[1]);
    // Send SLEEP operation to kernel scheduler
    t_package *pkg = package_create();
    pkg->op_code = SLEEP;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &time, sizeof(uint32_t));
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
    pthread_mutex_lock(&process_control_mutex);
    shoudld_stop = true; // Deberia parar por generar interrupcion de IO
    pthread_mutex_unlock(&process_control_mutex);
}

void instruction_stdout(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    // Send STDOUT operation to kernel scheduler with logical address and size registers
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid STDOUT register operand: %s", decoded_instruction[1]);
        return;
    }
    if(!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid STDOUT register operand: %s", decoded_instruction[2]);
        return;
    }

    uint32_t logical_address = read_register_value(context, decoded_instruction[1]);
    uint32_t size = read_register_value(context, decoded_instruction[2]);

    t_package *pkg = package_create();
    pkg->op_code = STDOUT;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &logical_address, sizeof(uint32_t));
    package_add(pkg, &size, sizeof(uint32_t));
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);

    pthread_mutex_lock(&process_control_mutex);
    shoudld_stop = true; // Deberia parar por generar interrupcion de IO
    pthread_mutex_unlock(&process_control_mutex);
}

void instruction_stdin(char **decoded_instruction, t_cpu_context *context, uint32_t pid) {
    // Send STDIN operation to kernel scheduler with logical address and size registers
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid STDIN register operand: %s", decoded_instruction[1]);
        return;
    }
    if(!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid STDIN register operand: %s", decoded_instruction[2]);
        return;
    }

    uint32_t logical_address = read_register_value(context, decoded_instruction[1]);
    uint32_t size = read_register_value(context, decoded_instruction[2]);

    t_package *pkg = package_create();
    pkg->op_code = STDIN;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &logical_address, sizeof(uint32_t));
    package_add(pkg, &size, sizeof(uint32_t));
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);

    pthread_mutex_lock(&process_control_mutex);
    shoudld_stop = true; // Deberia parar por generar interrupcion de IO
    pthread_mutex_unlock(&process_control_mutex);
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
// Un segmento en la tabla del proceso

// Tabla de segmentos del proceso (deberia llegar de Kernel Memory)
typedef struct {
    t_segment *segments;
    uint32_t   count;
} t_segment_table;

/*
// Devuelve la dirección física, o -1 si hay segfault
int32_t mmu_translate(t_cpu_context *context, uint32_t logical_addr,
                      uint32_t size, uint32_t segment_max_size) {
    uint32_t num_segmento   = logical_addr / segment_max_size;
    uint32_t desplazamiento = logical_addr % segment_max_size;

    // Buscar el segmento por índice en la tabla
    if (num_segmento >= context->segment_table.count) {
        log_error(logger, "SEG_FAULT PID %d: segmento %d no existe",
                  context->pid, num_segmento);
        return -1;
    }

    t_segment seg = context->segment_table.segments[num_segmento];

    if (desplazamiento + size > seg.limit) {
        log_error(logger, "SEG_FAULT PID %d: acceso fuera del segmento %d "
                  "(desp=%d, size=%d, limit=%d)",
                  context->pid, num_segmento, desplazamiento, size, seg.limit);
        return -1;
    }

    return (int32_t)(seg.base + desplazamiento);
}
// Lee `size` bytes desde dirección física, partiendo entre sticks si hace falta
// Devuelve 0 OK, -1 error
int mmu_read(uint32_t physical_addr, uint32_t size, void *buffer,
             uint32_t stick_size, int *memory_stick_fds, uint32_t stick_count) {
    uint32_t bytes_read = 0;

    while (bytes_read < size) {
        uint32_t cur_addr  = physical_addr + bytes_read;
        uint32_t stick_idx = cur_addr / stick_size;
        uint32_t stick_off = cur_addr % stick_size;

        if (stick_idx >= stick_count) {
            log_error(logger, "mmu_read: dirección física %d fuera de rango", cur_addr);
            return -1;
        }

        // Cuánto puedo leer de este stick sin pasarme
        uint32_t available  = stick_size - stick_off;
        uint32_t chunk_size = (size - bytes_read) < available
                              ? (size - bytes_read)
                              : available;

        // Armar paquete de lectura hacia el Memory Stick
        t_package *pkg = package_create();
        pkg->op_code = MEMORY_READ;
        package_add(pkg, &stick_off,   sizeof(uint32_t));
        package_add(pkg, &chunk_size,  sizeof(uint32_t));
        package_send(pkg, memory_stick_fds[stick_idx], NULL); // ???????
        package_delete(pkg);

        // Recibir los bytes leídos
        // (asumiendo que recibes un buffer con los bytes del stick)
        uint32_t bytes_received = 0;
        void *chunk = receive_buffer(memory_stick_fds[stick_idx], &bytes_received);
        if (chunk == NULL || bytes_received != chunk_size) {
            log_error(logger, "mmu_read: error leyendo del stick %d", stick_idx);
            free(chunk);
            return -1;
        }

        memcpy((uint8_t *)buffer + bytes_read, chunk, chunk_size);
        free(chunk);

        log_info(logger, "PID: - Acción: LEER - Dirección Física: %d - Valor: %.*s",
                 cur_addr, chunk_size, (char*)((uint8_t*)buffer + bytes_read));

        bytes_read += chunk_size;
    }

    return 0;
}

// Escribe `size` bytes en dirección física, partiendo entre sticks si hace falta
int mmu_write(uint32_t physical_addr, uint32_t size, void *buffer,
              uint32_t stick_size, int *memory_stick_fds, uint32_t stick_count) {
    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t cur_addr  = physical_addr + bytes_written;
        uint32_t stick_idx = cur_addr / stick_size;
        uint32_t stick_off = cur_addr % stick_size;

        if (stick_idx >= stick_count) {
            log_error(logger, "mmu_write: dirección física %d fuera de rango", cur_addr);
            return -1;
        }

        uint32_t available  = stick_size - stick_off;
        uint32_t chunk_size = (size - bytes_written) < available
                              ? (size - bytes_written)
                              : available;

        t_package *pkg = package_create();
        pkg->op_code = MEMORY_WRITE;
        package_add(pkg, &stick_off, sizeof(uint32_t));
        package_add(pkg, &chunk_size, sizeof(uint32_t));
        package_add(pkg, (uint8_t *)buffer + bytes_written, chunk_size);
        package_send(pkg, memory_stick_fds[stick_idx]);
        package_delete(pkg);

        // Esperar confirmación
        int ok = receive_ack(memory_stick_fds[stick_idx]);
        if (!ok) {
            log_error(logger, "mmu_write: error escribiendo en stick %d", stick_idx);
            return -1;
        }

        log_info(logger, "PID: - Acción: ESCRIBIR - Dirección Física: %d - Valor: %.*s",
                 cur_addr, chunk_size, (char*)((uint8_t*)buffer + bytes_written));

        bytes_written += chunk_size;
    }

    return 0;
}

*/