#include<instructions.h>


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


void instruction_set(char **decoded_instruction, t_cpu_context *context){
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[1]);
        return;
    }
    if (strcmp(decoded_instruction[1], "ax") == 0){
        context->ax = atoi(decoded_instruction[2]);
    }
    else if (strcmp(decoded_instruction[1], "bx") == 0){
        context->bx = atoi(decoded_instruction[2]);
    }
    else if (strcmp(decoded_instruction[1], "cx") == 0){
        context->cx = atoi(decoded_instruction[2]);
    }
    else if (strcmp(decoded_instruction[1], "dx") == 0){
        context->dx = atoi(decoded_instruction[2]);
    }
    else if (strcmp(decoded_instruction[1], "eax") == 0){
        context->eax = atoi(decoded_instruction[2]);
    }
    else if (strcmp(decoded_instruction[1], "ebx") == 0){
        context->ebx = atoi(decoded_instruction[2]);
    }
    else if (strcmp(decoded_instruction[1], "ecx") == 0){
        context->ecx = atoi(decoded_instruction[2]);
    }
    else if (strcmp(decoded_instruction[1], "edx") == 0){
        context->edx = atoi(decoded_instruction[2]);
    }
    else if (strcmp(decoded_instruction[1], "si") == 0){
        context->si = atoi(decoded_instruction[2]);
    }
    else if (strcmp(decoded_instruction[1], "di") == 0){
        context->di = atoi(decoded_instruction[2]);
    }
}

void instruction_sum(char **decoded_instruction, t_cpu_context *context){
    uint32_t add=0;
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[1]);
        return;
    }else if (!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[2]);
        return;
    }
    if (strcmp(decoded_instruction[2], "ax") == 0){
        add=context->ax; 
    }
    else if (strcmp(decoded_instruction[2], "bx") == 0){
        add=context->bx; 
    }
    else if (strcmp(decoded_instruction[2], "cx") == 0){
        add=context->cx; 
    }
    else if (strcmp(decoded_instruction[2], "dx") == 0){
        add=context->dx; 
    }
    else if (strcmp(decoded_instruction[2], "eax") == 0){
        add=context->eax; 
    }
    else if (strcmp(decoded_instruction[2], "ebx") == 0){
        add=context->ebx; 
    }
    else if (strcmp(decoded_instruction[2], "ecx") == 0){
        add=context->ecx; 
    }
    else if (strcmp(decoded_instruction[2], "edx") == 0){
        add=context->edx; 
    }
    else if (strcmp(decoded_instruction[2], "si") == 0){
        add=context->si; 
    }
    else if (strcmp(decoded_instruction[2], "di") == 0){
        add=context->di; 
    }


    if (strcmp(decoded_instruction[1], "ax") == 0){ 
        context->ax += add;
    }
    else if (strcmp(decoded_instruction[1], "bx") == 0){
        context->bx += add;
    }
    else if (strcmp(decoded_instruction[1], "cx") == 0){
        context->cx += add;
    }
    else if (strcmp(decoded_instruction[1], "dx") == 0){
        context->dx += add;
    }
    else if (strcmp(decoded_instruction[1], "eax") == 0){
        context->eax += add;
    }
    else if (strcmp(decoded_instruction[1], "ebx") == 0){
        context->ebx += add;
    }
    else if (strcmp(decoded_instruction[1], "ecx") == 0){
        context->ecx += add;
    }
    else if (strcmp(decoded_instruction[1], "edx") == 0){
        context->edx += add;
    }
    else if (strcmp(decoded_instruction[1], "si") == 0){
        context->si += add;
    }
    else if (strcmp(decoded_instruction[1], "di") == 0){
        context->di += add;
    }
}

void instruction_sub(char **decoded_instruction, t_cpu_context *context){
    uint32_t add=0;
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[1]);
        return;
    }else if (!check_if_register(decoded_instruction[2])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[2]);
        return;
    }
    if (strcmp(decoded_instruction[2], "ax") == 0){
        add=context->ax; 
    }
    else if (strcmp(decoded_instruction[2], "bx") == 0){
        add=context->bx; 
    }
    else if (strcmp(decoded_instruction[2], "cx") == 0){
        add=context->cx; 
    }
    else if (strcmp(decoded_instruction[2], "dx") == 0){
        add=context->dx; 
    }
    else if (strcmp(decoded_instruction[2], "eax") == 0){
        add=context->eax; 
    }
    else if (strcmp(decoded_instruction[2], "ebx") == 0){
        add=context->ebx; 
    }
    else if (strcmp(decoded_instruction[2], "ecx") == 0){
        add=context->ecx; 
    }
    else if (strcmp(decoded_instruction[2], "edx") == 0){
        add=context->edx; 
    }
    else if (strcmp(decoded_instruction[2], "si") == 0){
        add=context->si; 
    }
    else if (strcmp(decoded_instruction[2], "di") == 0){
        add=context->di; 
    }


    if (strcmp(decoded_instruction[1], "ax") == 0){ 
        context->ax -= add;
    }
    else if (strcmp(decoded_instruction[1], "bx") == 0){
        context->bx -= add;
    }
    else if (strcmp(decoded_instruction[1], "cx") == 0){
        context->cx -= add;
    }
    else if (strcmp(decoded_instruction[1], "dx") == 0){
        context->dx -= add;
    }
    else if (strcmp(decoded_instruction[1], "eax") == 0){
        context->eax -= add;
    }
    else if (strcmp(decoded_instruction[1], "ebx") == 0){
        context->ebx -= add;
    }
    else if (strcmp(decoded_instruction[1], "ecx") == 0){
        context->ecx -= add;
    }
    else if (strcmp(decoded_instruction[1], "edx") == 0){
        context->edx -= add;
    }
    else if (strcmp(decoded_instruction[1], "si") == 0){
        context->si -= add;
    }
    else if (strcmp(decoded_instruction[1], "di") == 0){
        context->di -= add;
    }
}

void instruction_jnz(char **decoded_instruction, t_cpu_context *context){
    if (!check_if_register(decoded_instruction[1])) {
        log_error(logger, "Invalid register: %s", decoded_instruction[1]);
        return;
    }
    if (atoi(decoded_instruction[2]<=0) {
        log_error(logger, "The program counter cannot be less than 1");
        return;
    }
    if (strcmp(decoded_instruction[1], "ax") == 0){
        if (context->ax!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
    }
    else if (strcmp(decoded_instruction[1], "bx") == 0){
        if (context->bx!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
    }
    else if (strcmp(decoded_instruction[1], "cx") == 0){
        if (context->cx!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
    }
    else if (strcmp(decoded_instruction[1], "dx") == 0){
        if (context->dx!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
    }
    else if (strcmp(decoded_instruction[1], "eax") == 0){
        if (context->eax!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
    }
    else if (strcmp(decoded_instruction[1], "ebx") == 0){
        if (context->ebx!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
    }
    else if (strcmp(decoded_instruction[1], "ecx") == 0){
        if (context->ecx!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
    }
    else if (strcmp(decoded_instruction[1], "edx") == 0){
        if (context->edx!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
    }
    else if (strcmp(decoded_instruction[1], "si") == 0){
        if (context->si!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
    }
    else if (strcmp(decoded_instruction[1], "di") == 0){
        if (context->di!=0) {
            context->pc = atoi(decoded_instruction[2]);
        }
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
			instruction_set(decoded_instruction, context)
			log_info(logger, "SET executed");
			break;
		}

		case MOV_IN:{} //NO ENTIENDO BIEN COMO FUNCIONA LA DIRECCION LOGICA
		case MOV_IN:{} //NO ENTIENDO BIEN COMO FUNCIONA LA DIRECCION LOGICA
		
		case SUM: {//SE PUEDE USAR SUMA EN SI O EN DI, UTILIZA SU VALOR O SU UBICADO, QUE PASA SI ES MAYOR QUE SU TIPO
			instruction_sum(decoded_instruction, context)
			log_info(logger, "SUM executed");
			break;
		}

        case SUB: {//SE PUEDE USAR SUB EN SI O EN DI, UTILIZA SU VALOR O SU UBICADO, QUE PASA SI ES MAYOR QUE SU TIPO O MENOR QUE 0
			instruction_sub(decoded_instruction, context)
			log_info(logger, "SUB executed");
			break;
		}

        case JNZ: {
			instruction_jnz(decoded_instruction, context)
			log_info(logger, "JNZ executed");
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
