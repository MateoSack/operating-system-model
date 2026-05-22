#include <main.h>

t_log *logger;

int kernel_scheduler_fd = -1;
int kernel_memory_fd = -1;
bool interruptPending = 0;
uint32_t cpu_id;
char *cpu_identifier = NULL;

pthread_mutex_t interrupt_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t memory_stick_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t kernel_scheduler_write_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t kernel_memory_write_mutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t kernel_memory_read_mutex = PTHREAD_MUTEX_INITIALIZER;

t_list *list_memory_stick;

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

	/*-------------------Connection with Kernel Scheduler-------------------*/
	if (connect_kernel_scheduler(logger, config) == EXIT_FAILURE)
		return EXIT_FAILURE;

	/*-------------------Connection with Kernel Memory-------------------*/
	if (connect_kernel_memory(logger, config) == EXIT_FAILURE)
		return EXIT_FAILURE;

	kernel_scheduler_handler(kernel_scheduler_fd, kernel_memory_fd);

	log_destroy(logger);
	config_destroy(config);
	free(cpu_identifier);
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
		pthread_mutex_lock(&kernel_memory_read_mutex);
		int op = operation_receive(kernel_memory_fd);
		if (op == -1)
		{
			pthread_mutex_unlock(&kernel_memory_read_mutex);
			log_warning(logger, "Kernel Memory disconnected");
			close(kernel_memory_fd);
			break;
		}
		switch (op) {
			case CREDENTIALS_UPDATE: {
				t_module_credentials *credentials = receive_credentials(kernel_memory_fd);
				pthread_mutex_unlock(&kernel_memory_read_mutex);
				log_debug(logger, "Received credentials: ip=%s, port=%s, id=%d", credentials->ip, credentials->port, credentials->id);
				connect_with_memory_stick(logger, credentials);
				break;
			}

			default: {
				pthread_mutex_unlock(&kernel_memory_read_mutex);
				log_warning(logger, "Received unknown operation code %d from Kernel Memory", op);
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
				uint32_t pid = uint32_decode(kernel_scheduler_fd);
				log_info(logger, "Received PID %d from Kernel scheduler", pid);
				
				t_package *pkg = package_create();
				pkg->op_code = CONTEXT_SEEK;
				package_add(pkg, &pid, sizeof(uint32_t));
				pthread_mutex_lock(&kernel_memory_write_mutex);
				package_send(pkg, kernel_memory_fd);
				pthread_mutex_unlock(&kernel_memory_write_mutex);
				package_delete(pkg);

				log_info(logger, "Sent CONTEXT_SEEK request to Kernel Memory");
				
				pthread_mutex_lock(&kernel_memory_read_mutex);
				t_cpu_context *context = context_receive(kernel_memory_fd);
				pthread_mutex_unlock(&kernel_memory_read_mutex);

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
				pthread_mutex_lock(&interrupt_mutex);
				interruptPending = 1; //ahora mismo no hay otros códigos de operación que reciba el scheduler implementados
				pthread_mutex_unlock(&interrupt_mutex);
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
	
	pthread_mutex_lock(&memory_stick_list_mutex);
	t_client_info *mem_stick = add_client_to_list(list_memory_stick, memory_stick_fd, credentials->id);
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
