#include <main.h>

t_log *logger;

t_client_info *kernel_scheduler = NULL;
t_client_info *kernel_memory = NULL;
bool interruptPending = 0;
t_interrupt_reason interruptReason = QUANTUM_EXPIRED;
uint32_t cpu_id;
char *cpu_identifier = NULL;

pthread_mutex_t interrupt_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t memory_stick_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t kernel_scheduler_write_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t kernel_memory_write_mutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t process_control_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t sem_instruction_fetch_ready;
sem_t sem_instruction_response_ready;
sem_t sem_eviction_ready;

t_instruction_response instruction_response = {
	.instruction = NULL,
	.is_ready = false,
	.mutex = PTHREAD_MUTEX_INITIALIZER
};

t_list *list_memory_stick;

// Pending request for context seek
typedef struct {
	uint32_t pid;
	t_cpu_context *context;
	sem_t sem;
	bool ready;
} t_pending_request;

t_pending_request *pending_request = NULL;
pthread_mutex_t pending_request_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {

	if (argc < 3) {
        printf("Mode of use: %s <config_file> <Identifier>\n", argv[0]);
        return EXIT_FAILURE;
    }

	/*-------------------Initial Setup-------------------*/
	t_config *config = config_create(argv[1]);
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);
	log_info(logger, "CPU started");
	list_memory_stick = list_create();
	cpu_identifier = strdup(argv[2]); 
	
	sem_init(&sem_instruction_fetch_ready, 0, 0);
	sem_init(&sem_instruction_response_ready, 0, 0); 
	sem_init(&sem_eviction_ready, 0, 0);

	/*-------------------Connection with Kernel Scheduler-------------------*/
	if (connect_kernel_scheduler(logger, config) == EXIT_FAILURE)
		return EXIT_FAILURE;

	/*-------------------Connection with Kernel Memory-------------------*/
	if (connect_kernel_memory(logger, config) == EXIT_FAILURE)
		return EXIT_FAILURE;

	kernel_scheduler_handler(kernel_scheduler);

	log_destroy(logger);
	config_destroy(config);
	free(cpu_identifier);
	return EXIT_SUCCESS;
}

int connect_kernel_memory(t_log *logger, t_config *config) {
	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	log_debug(logger, "Attempting connection with ip: %s, port: %s", kernel_memory_ip, kernel_memory_port);
	int kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	if (kernel_memory_fd == -1) {
		log_info(logger, "Couldnt connect with Kernel Memory");
		return EXIT_FAILURE;
	}

	kernel_memory = create_client_info(kernel_memory_fd, 0); // ID is not relevant for kernel memory, set to 0

	t_module_id_send(kernel_memory->fd, MODULE_CPU, logger, &kernel_memory->network_mutex);
	uint32_send(kernel_memory->fd, cpu_id, &kernel_memory->network_mutex);

	log_info(logger, "Connection successful with Kernel Memory");

	t_list *credentials_list = receive_credentials_list(kernel_memory->fd);
	log_info(logger, "Received credentials list from Kernel Memory with %d entries", list_size(credentials_list));
	if (list_size(credentials_list) != 0) {
		if (iterate_connection_create_with_memory_sticks(credentials_list) == EXIT_FAILURE)
			return EXIT_FAILURE;
	} else {
		log_info(logger, "Credentials list empty.");
	}
	// list_destroy_and_destroy_elements(credentials_list, t_memory_stick_credentials_destroyer);

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
	int kernel_scheduler_fd = connection_create(kernel_scheduler_ip, kernel_scheduler_port, logger);

	if (kernel_scheduler_fd == -1)
	{
		log_error(logger, "Connection attempt to Kernel scheduler failed.");
		return EXIT_FAILURE;
	}

	kernel_scheduler = create_client_info(kernel_scheduler_fd, 0); // ID is not relevant for kernel scheduler, set to 0

	log_debug(logger, "Attempting to send t_module_id to %d", kernel_scheduler_fd);
	t_module_id_send(kernel_scheduler->fd, MODULE_CPU, logger, &kernel_scheduler->network_mutex);

	cpu_id = uint32_receive(kernel_scheduler->fd);
	log_info(logger, "Connection successful to Kernel Scheduler, CPU ID: %d", cpu_id);

	free(kernel_scheduler_ip);
	free(kernel_scheduler_port);
	return EXIT_SUCCESS;
}

void *kernel_memory_thread()
{
	while (1) {
		// Centralized reader for Kernel Memory
		int op = operation_receive(kernel_memory->fd);
		if (op == -1) {
			log_error(logger, "Kernel Memory disconnected");
			close(kernel_scheduler->fd);
			close(kernel_memory->fd);

			destroy_client(kernel_scheduler);
			exit(EXIT_FAILURE);
		}

		switch (op) {
			case INSTRUCTION_FETCH: {
				// CPU instruction fetch flow: wait until CPU thread signals readiness
				log_debug(logger, "Received INSTRUCTION_FETCH request from CPU thread, waiting for CPU to be ready");
				sem_wait(&sem_instruction_fetch_ready);
				log_debug(logger, "CPU thread is ready for instruction, receiving instruction from Kernel Memory");
				char *instruction = message_decode(kernel_memory->fd);
				log_debug(logger, "Instruction received from Kernel Memory");

				pthread_mutex_lock(&instruction_response.mutex);
				instruction_response.instruction = instruction;
				instruction_response.is_ready = true;
				pthread_mutex_unlock(&instruction_response.mutex);

				log_debug(logger, "Received instruction: %s", instruction);
				sem_post(&sem_instruction_response_ready);
				break;
			}

			case CONTEXT_TRANSFER: {
				// Received context for a previous CONTEXT_SEEK -> dispatch to pending requester
				t_cpu_context *ctx = context_receive(kernel_memory->fd);

				pthread_mutex_lock(&pending_request_mutex);
				if (pending_request != NULL) {
					pending_request->context = ctx;
					pending_request->ready = true;
					sem_post(&pending_request->sem);
				} else {
					free(ctx);
					log_warning(logger, "Received CONTEXT_TRANSFER but no pending request");
				}
				pthread_mutex_unlock(&pending_request_mutex);
				break;
			}

			case CREDENTIALS_UPDATE: {
				t_memory_stick_credentials *credentials = receive_credentials(kernel_memory->fd);
				log_debug(logger, "Received credentials: ip=%s, port=%s, id=%d", credentials->ip, credentials->port, credentials->id);
				connect_with_memory_stick(logger, credentials);
				break;
			}

			default: {
				log_warning(logger, "Received unknown operation code %d from Kernel Memory", op);
				break;
			}
		}
	}
	return NULL;
}

void kernel_scheduler_handler(t_client_info *kernel_scheduler)
{
	while (1)
	{
		// Handle connection with Kernel Scheduler
		int op = operation_receive(kernel_scheduler->fd);
		if (op == -1)
		{
			log_error(logger, "Kernel Scheduler disconnected");
			destroy_client(kernel_scheduler);
			destroy_client(kernel_memory);
			exit(EXIT_FAILURE);
		}
		switch (op) {
			case PROCESS_EXECUTE:
			{
				pthread_mutex_lock(&interrupt_mutex);
    			bool hay_interrupcion = interruptPending;
   				pthread_mutex_unlock(&interrupt_mutex);

    			if (hay_interrupcion) {
        			sem_wait(&sem_eviction_ready);
    			}

				uint32_t pid = uint32_decode(kernel_scheduler->fd);
				log_info(logger, "Received PID %d from Kernel scheduler", pid);

				// Create pending request before sending CONTEXT_SEEK so the response can be delivered immediately.
				pthread_mutex_lock(&pending_request_mutex);
				if (pending_request != NULL) {
					log_error(logger, "Unexpected pending request already exists");
				}
				pending_request = malloc(sizeof(t_pending_request));
				pending_request->pid = pid;
				pending_request->context = NULL;
				pending_request->ready = false;
				sem_init(&pending_request->sem, 0, 0);
				pthread_mutex_unlock(&pending_request_mutex);

				t_package *pkg = package_create();
				pkg->op_code = CONTEXT_SEEK;
				package_add(pkg, &pid, sizeof(uint32_t));
				pthread_mutex_lock(&kernel_memory_write_mutex);
				package_send(pkg, kernel_memory->fd, &kernel_memory->network_mutex);
				pthread_mutex_unlock(&kernel_memory_write_mutex);
				package_delete(pkg);

				log_info(logger, "Sent CONTEXT_SEEK request to Kernel Memory");

				// Wait until kernel_memory_thread posts the context
				sem_wait(&pending_request->sem);
				t_cpu_context *context = pending_request->context;

				pthread_mutex_lock(&pending_request_mutex);
				sem_destroy(&pending_request->sem);
				free(pending_request);
				pending_request = NULL;
				pthread_mutex_unlock(&pending_request_mutex);

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

			case PROCESS_EVICT: {
				int size;
				int offset = 0;
				void *buffer = buffer_receive(&size, kernel_scheduler->fd);
				if (buffer == NULL) {
					log_error(logger, "Failed to receive PROCESS_EVICT payload");
					break;
				}
				uint32_t reason_val = uint32_deserialize(buffer, &offset);
				free(buffer);
				t_interrupt_reason reason = (t_interrupt_reason)reason_val;
				log_info(logger, "Received PROCESS_EVICT (reason=%s)", interrupt_reason_to_string(reason));

				pthread_mutex_lock(&interrupt_mutex);
				interruptPending = 1;
				interruptReason = reason;
				pthread_mutex_unlock(&interrupt_mutex);
				break;
			}
			default: {
				log_warning(logger, "Received unknown operation code %d from Kernel Scheduler", op);
				break;
			}
		}
	}
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
		t_memory_stick_credentials *credentials = list_get(list, i);
		if (connect_with_memory_stick(logger, credentials) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}

	log_debug(logger, "Finished stablishing connectios with memory sticks, total: %d", i);
	return EXIT_SUCCESS;
}

int connect_with_memory_stick(t_log *logger, t_memory_stick_credentials *credentials)
{
	log_debug(logger, "Attempting connection with ip: %s, port: %s", credentials->ip, credentials->port);
	int memory_stick_fd = connection_create(credentials->ip, credentials->port, logger);

	if (memory_stick_fd == -1) {
		log_error(logger, "Couldnt connect with Memory Stick");
		return EXIT_FAILURE;
	}

	t_client_info *mem_stick = create_client_info(memory_stick_fd, credentials->id);

	t_module_id_send(memory_stick_fd, MODULE_CPU, logger, &mem_stick->network_mutex);
	uint32_send(memory_stick_fd, cpu_id, &mem_stick->network_mutex);
	log_debug(logger, "Sent MODULE_CPU and cpu_id = %d to Memory Stick", cpu_id);
	
	pthread_mutex_lock(&memory_stick_list_mutex);
	list_add(list_memory_stick, mem_stick);
	int mem_stick_count = list_size(list_memory_stick);
	pthread_mutex_unlock(&memory_stick_list_mutex);

	log_info(logger, "Memory Stick %d connected (total: %d)", credentials->id, mem_stick_count);

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
			pthread_mutex_lock(&memory_stick_list_mutex);
			list_remove_element(list_memory_stick, mem_stick);
			pthread_mutex_unlock(&memory_stick_list_mutex);
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
