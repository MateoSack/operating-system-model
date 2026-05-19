#include <main.h>

t_log *logger;

int kernel_scheduler_fd = -1;
int kernel_memory_fd = -1;
bool interruptPending = 0;
uint32_t cpu_id;
uint32_t pid;

t_cpu_context *context = NULL;

t_list *list_memory_stick;

int main(void) {
	/*-------------------Initial Setup-------------------*/
	t_config *config = config_create("cpu.config");
	if (config == NULL)
		return EXIT_FAILURE;
	logger = start_logger(config);
	log_info(logger, "CPU started");
	context = malloc(sizeof(t_cpu_context));
	list_memory_stick = list_create();

	/*-------------------Connection with Kernel Scheduler-------------------*/
	if (connect_kernel_scheduler(logger, config) == EXIT_FAILURE)
		return EXIT_FAILURE;

	/*-------------------Connection with Kernel Memory-------------------*/
	if (connect_kernel_memory(logger, config) == EXIT_FAILURE)
		return EXIT_FAILURE;

	kernel_scheduler_handler(kernel_scheduler_fd, kernel_memory_fd);

	log_destroy(logger);
	config_destroy(config);
	return EXIT_SUCCESS;
}

int connect_kernel_memory(t_log *logger, t_config *config) {
	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	log_debug(logger, "Attempting connection with ip: %s, port: %s", kernel_memory_ip, kernel_memory_port);
	kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	if (kernel_memory_fd == -1) {
		log_info(logger, "Couldnt connect with Kernel Memory");
		return EXIT_FAILURE;
	}

	t_module_id_send(kernel_memory_fd, MODULE_CPU, logger);
	uint32_send(kernel_memory_fd, cpu_id);

	log_info(logger, "Connection successful with Kernel Memory");

	t_list *credentials_list = receive_credentials_list(kernel_memory_fd);
	log_debug(logger, "Has received credentials' list");
	if (list_size(credentials_list) != 0) {
		if (iterate_connection_create_with_memory_sticks(credentials_list) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}
	else
	{
		log_debug(logger, "Credentials list empty.");
	}
	// list_destroy_and_destroy_elements(credentials_list, t_module_credentials_destroyer);

	pthread_t thread;
	pthread_create(&thread, NULL, kernel_memory_thread, NULL);
	pthread_detach(thread);

	free(kernel_memory_ip);
	free(kernel_memory_port);
	return EXIT_SUCCESS;
}

int connect_kernel_scheduler(t_log *logger, t_config *config)
{
	char *kernel_scheduler_ip = config_get_string_value(config, "KERNEL_SCHEDULER_IP");
	char *kernel_scheduler_port = config_get_string_value(config, "KERNEL_SCHEDULER_PORT");

	log_debug(logger, "Attempting connection with %s:%s", kernel_scheduler_ip, kernel_scheduler_port);
	kernel_scheduler_fd = connection_create(kernel_scheduler_ip, kernel_scheduler_port, logger);

	if (kernel_scheduler_fd == -1)
	{
		log_error(logger, "Connection attempt to Kernel scheduler failed.");
		return EXIT_FAILURE;
	}

	log_debug(logger, "Attempting to send t_module_id to %d", kernel_scheduler_fd);
	t_module_id_send(kernel_scheduler_fd, MODULE_CPU, logger);

	cpu_id = uint32_receive(kernel_scheduler_fd);
	log_info(logger, "Connection successful to Kernel Scheduler, CPU ID: %d", cpu_id);

	free(kernel_scheduler_ip);
	free(kernel_scheduler_port);
	return EXIT_SUCCESS;
}

void *kernel_memory_thread()
{
	while (1)
	{
		// Handle connection with Kernel Memory
		int op = operation_receive(kernel_memory_fd);
		log_debug(logger, "Attending op_code: %d", op);
		if (op == -1)
		{
			log_warning(logger, "Kernel Memory disconnected");
			close(kernel_memory_fd);
			break;
		}
		switch (op) {
			case CREDENTIALS_UPDATE: {
				t_module_credentials *credentials = receive_credentials(kernel_memory_fd);
				log_debug(logger, "Received credentials: ip=%s, port=%s, id=%d", credentials->ip, credentials->port, credentials->id);
				connect_with_memory_stick(logger, credentials);
				break;
			}
		}
	}
	return NULL;
}

void kernel_scheduler_handler(int kernel_scheduler_fd, int kernel_memory_fd)
{
	while (1)
	{
		// Handle connection with Kernel Scheduler
		int op = operation_receive(kernel_scheduler_fd);
		if (op == -1)
		{
			log_warning(logger, "Kernel Scheduler disconnected");
			close(kernel_scheduler_fd);
			break;
		}
		switch (op) {
			case PROCESS_EXECUTE:
			{
				pid = uint32_decode(kernel_scheduler_fd);
				log_info(logger, "Received PID %d from Kernel scheduler", pid);
				
				t_package *pkg = package_create();
				pkg->op_code = CONTEXT_SEEK;
				package_add(pkg, &pid, sizeof(uint32_t));
				package_send(pkg, kernel_memory_fd);
				package_delete(pkg);
				log_info(logger, "Sent CONTEXT_SEEK request to Kernel Memory");
				
				context = malloc(sizeof(t_cpu_context));
				*context = *context_receive(kernel_memory_fd);
				if (context == NULL) {
					log_error(logger, "Failed to receive context from Kernel Memory");
					break;
				}
				log_info(logger, "Context received correctly from Kernel Memory for PID %d", pid);
				
				t_process_execution_args *execution_args = malloc(sizeof(t_process_execution_args));
				execution_args->pid = pid;
				execution_args->context = context;
				
				pthread_t thread;
				pthread_create(&thread, NULL, process_execution_handler, execution_args);
				pthread_detach(thread);
				
				break;
			}

			default: {
				interruptPending = 1; //ahora mismo no hay otros códigos de operación que reciba el scheduler implementados
				break;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
// Toda esta zona podría estar en un archivo aparte de instructions_utils o instructions_cicle o algo del estilo
//--------------------------------------------------------------------------------------------------------------
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

void execute_instruction(char **decoded_instruction, t_cpu_context *context) {
	t_instruction_type instruction = instruction_to_type(decoded_instruction[0]);
	
	switch (instruction) {
		case NO_OP: {
			log_info(logger, "NO_OP executed");
			break;
		}

		case SET: {
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
			log_info(logger, "SET executed");
			break;
		}

		case MOV_IN: //TODO: hacer todos estos y los SYSCALLS
			log_info(logger, "MOV_IN executed");
			break;
		case MOV_OUT:
			log_info(logger, "MOV_OUT executed");
			break;
		case SUM:
			log_info(logger, "SUM executed");
			break;
		case SUB:
			log_info(logger, "SUB executed");
			break;
		case JNZ:
			log_info(logger, "JNZ executed");
			break;
		case COPY_MEM:
			log_info(logger, "COPY_MEM executed");
			break;
		case UNKNOWN:
			log_warning(logger, "Unknown instruction: %s", decoded_instruction[0]);
			break;
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

t_log *start_logger(t_config *config)
{
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("cpu.log", "CPU", true, level);
	return logger;
}

int iterate_connection_create_with_memory_sticks(t_list *list)
{
	int i;
	for (i = 0; i < list_size(list); i++)
	{
		t_module_credentials *credentials = list_get(list, i);
		if (connect_with_memory_stick(logger, credentials) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}

	log_debug(logger, "Finished stablishing connectios with memory sticks, total: %d", i);
	return EXIT_SUCCESS;
}

int connect_with_memory_stick(t_log *logger, t_module_credentials *credentials)
{
	log_debug(logger, "Attempting connection with ip: %s, port: %s", credentials->ip, credentials->port);
	int memory_stick_fd = connection_create(credentials->ip, credentials->port, logger);

	if (memory_stick_fd == -1)
	{
		log_error(logger, "Couldnt connect with Memory Stick");
		return EXIT_FAILURE;
	}

	t_module_id_send(memory_stick_fd, MODULE_CPU, logger);
	uint32_send(memory_stick_fd, cpu_id);
	log_debug(logger, "Sent MODULE_CPU and cpu_id = %d to Memory Stick", cpu_id);

	t_client_info *mem_stick = add_client_to_list(list_memory_stick, memory_stick_fd, credentials->id);

	log_info(logger, "Memory Stick %d connected (total: %d)", credentials->id, list_size(list_memory_stick));

	pthread_t thread;
	pthread_create(&thread, NULL, memory_stick_handler, mem_stick);
	pthread_detach(thread);

	return EXIT_SUCCESS;
}

void *memory_stick_handler(void *mem_stick_ptr)
{
	t_client_info *mem_stick = (t_client_info *)mem_stick_ptr;
	log_debug(logger, "Memory Stick handler started for fd: %d, id: %d", mem_stick->fd, mem_stick->id);

	while (1)
	{
		// Handle connection with Memory Stick
		int op = operation_receive(mem_stick->fd);
		if (op == -1)
		{
			log_warning(logger, "Memory Stick %d disconnected", mem_stick->id);
			close(mem_stick->fd);
			list_remove_element(list_memory_stick, mem_stick);
			free(mem_stick);
			break;
		}
	}
	return NULL;
}

/*
void end_program(t_log *logger, t_config *config)
{
	log_destroy(logger);

	config_destroy(config);

}
*/
