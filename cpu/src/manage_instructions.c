#include<manage_instructions.h>

void instructions_cicle(t_cpu_context *context, uint32_t pid) {
	while (1)
	{
		t_package *pkg = package_create();
		pkg->op_code = INSTRUCTION_FETCH;
		package_add(pkg, &pid, sizeof(uint32_t));
		package_send(pkg, kernel_memory_fd);
		package_delete(pkg);
		log_info(logger, "Sent INSTRUCTION_FETCH request to Kernel Memory");
		char *instruction = message_receive(logger, kernel_memory_fd); // a chequear si estan bien los parametros
		if (instruction == NULL) {
			log_error(logger, "Failed to receive instruction from Kernel Memory");
			context->pc++;
			break;
		}
		log_info(logger, "Received instruction from Kernel Memory: %s", instruction);
		char **decoded_instruction = decode_instruction(instruction);
		execute_instruction(decoded_instruction, context);
		log_info(logger, "Executed instruction: %s", instruction);
		free(instruction);
		string_array_destroy(decoded_instruction);
		context->pc++;

		if(interruptPending) {
			log_info(logger, "Interrupt pending for PID %d, sending context to Kernel Memory", pid);
			t_package *pkg = package_create();
			pkg->op_code = CONTEXT_TRANSFER;
			package_add(pkg, &pid, sizeof(uint32_t));
			package_send(pkg, kernel_memory_fd);
			context_send(context, kernel_memory_fd);
			package_delete(pkg);
			log_info(logger, "Context saved to Kernel Memory");
			interruptPending = 0;
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
        return false;
    }

    if (descriptor.field_size == sizeof(uint8_t)) {
        *(uint8_t *)descriptor.field_address = (uint8_t)value;
    } else {
        *(uint32_t *)descriptor.field_address = value;
    }
    return true;
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
        log_error(logger, "Invalid register: %s", decoded_instruction[1]);
        return;
    }
    if (!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[2]);
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
        log_error(logger, "Invalid register: %s", decoded_instruction[1]);
        return;
    }
    if (!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[2]);
        return;
    }

    uint32_t left_value = read_register_value(context, decoded_instruction[1]);
    uint32_t right_value = read_register_value(context, decoded_instruction[2]);

    if (!write_register_value(context, decoded_instruction[1], left_value - right_value)) {
        log_error(logger, "Failed to write SUB result to register: %s", decoded_instruction[1]);
    }
}

void instruction_jnz(char **decoded_instruction, t_cpu_context *context){
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[1]);
        return;
    }
    if (atoi(decoded_instruction[2]) <= 0) {
        log_error(logger, "The program counter cannot be less than 1");
        return;
    }

    if (read_register_value(context, decoded_instruction[1]) != 0) {
        context->pc = atoi(decoded_instruction[2]);
    }
}

void execute_instruction(char **decoded_instruction, t_cpu_context *context) {
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

		case MOV_IN: //NO ENTIENDO BIEN COMO FUNCIONA LA DIRECCION LOGICA
		case MOV_OUT: //NO ENTIENDO BIEN COMO FUNCIONA LA DIRECCION LOGICA
			log_info(logger, "%s operation not implemented", decoded_instruction[0]);
			break;
		
		case SUM: {//SE PUEDE USAR SUMA EN SI O EN DI, UTILIZA SU VALOR O SU UBICADO, QUE PASA SI ES MAYOR QUE SU TIPO
			instruction_sum(decoded_instruction, context);
			log_info(logger, "SUM executed");
			break;
		}

        case SUB: {//SE PUEDE USAR SUB EN SI O EN DI, UTILIZA SU VALOR O SU UBICADO, QUE PASA SI ES MAYOR QUE SU TIPO O MENOR QUE 0
			instruction_sub(decoded_instruction, context);
			log_info(logger, "SUB executed");
			break;
		}

        case JNZ: {
			instruction_jnz(decoded_instruction, context);
			log_info(logger, "JNZ executed");
			break;
		}

        case COPY_MEM: {
            break;
        }
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
