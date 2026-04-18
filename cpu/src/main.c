#include <main.h>

t_log *logger;

int kernel_scheduler_fd = -1;
int kernel_memory_fd = -1;
uint32_t cpu_id;

t_list *list_memory_stick;

int main(void)
{
	/*-------------------Initial Setup-------------------*/
	t_config *config = config_create("cpu.config");
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);	
	log_info(logger, "CPU started");
	list_memory_stick = list_create();

	/*-------------------Connection with Kernel Scheduler-------------------*/
	if(connect_kernel_scheduler(logger, config) == EXIT_FAILURE) return EXIT_FAILURE;

	/*-------------------Connection with Kernel Memory-------------------*/
	if(connect_kernel_memory(logger, config) == EXIT_FAILURE) return EXIT_FAILURE;

	kernel_scheduler_handler(kernel_scheduler_fd);

	log_destroy(logger);
    config_destroy(config);
	return EXIT_SUCCESS;
}

int connect_kernel_memory (t_log *logger, t_config *config) {
	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	log_debug(logger, "Attempting connection with ip: %s, port: %s", kernel_memory_ip, kernel_memory_port);
	kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	if (kernel_memory_fd == -1) {
		log_info(logger, "Couldnt connect with Kernel Memory");
		return EXIT_FAILURE;
	}

	t_module_id_send (kernel_memory_fd, MODULE_CPU, logger);
	uint32_send(kernel_memory_fd, cpu_id);

	log_info(logger, "Connection successful with Kernel Memory");

	t_list *credentials_list = receive_credentials_list(kernel_memory_fd);
	log_debug(logger, "Has received credentials' list");
	if(list_size(credentials_list) != 0) {
		if(iterate_connection_create_with_memory_sticks (credentials_list) == EXIT_FAILURE) return EXIT_FAILURE;
	} else {
		log_debug(logger, "Credentials' list empty");
	}
	//list_destroy_and_destroy_elements(credentials_list, t_module_credentials_destroyer);

	pthread_t thread;
	pthread_create(&thread, NULL, kernel_memory_thread, NULL);
	pthread_detach(thread);

	free(kernel_memory_ip);
	free(kernel_memory_port);
	return EXIT_SUCCESS;
}

int connect_kernel_scheduler(t_log *logger, t_config *config) {
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

void *kernel_memory_thread () {
	while (1) {
		//Handle connection with Kernel Memory
		int op = operation_receive(kernel_memory_fd);
		log_debug(logger, "Attending op_code: %d", op);
		if (op == -1) {
			log_warning(logger, "Kernel Memory disconnected");
			close(kernel_memory_fd);
			break;
		} else if (op == CREDENTIALS_UPDATE) {
			t_module_credentials *credentials = receive_credentials (kernel_memory_fd);
			log_debug(logger, "Received credentials: ip=%s, port=%s, id=%d", credentials->ip, credentials->port, credentials->id);
			connect_with_memory_stick(logger, credentials);
		}
	}
	return NULL;
}

void kernel_scheduler_handler (int kernel_scheduler_fd) {
	while (1) {
		//Handle connection with Kernel Scheduler
		int op = operation_receive(kernel_scheduler_fd);
		if (op == -1) {
			log_warning(logger, "Kernel Scheduler disconnected");
			close(kernel_scheduler_fd);
			break;
		}
	}
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("cpu.log", "CPU", true, level);
	return logger;
}

int iterate_connection_create_with_memory_sticks (t_list *list) {
	int i;
	for(i = 0; i < list_size(list); i++) {
		t_module_credentials *credentials = list_get(list, i);
		if(connect_with_memory_stick(logger, credentials) == EXIT_FAILURE) return EXIT_FAILURE;
	}

	log_debug(logger, "Finished stablishing connectios with memory sticks, total: %d", i);
	return EXIT_SUCCESS;
}

int connect_with_memory_stick (t_log *logger, t_module_credentials *credentials) {
	log_debug(logger, "Attempting connection with ip: %s, port: %s", credentials->ip, credentials->port);
	int memory_stick_fd = connection_create(credentials->ip, credentials->port, logger);

	if (memory_stick_fd == -1) {
		log_error(logger, "Couldnt connect with Memory Stick");
		return EXIT_FAILURE;
	}

	t_module_id_send (memory_stick_fd, MODULE_CPU, logger);
	uint32_send(memory_stick_fd, cpu_id);
	log_debug(logger, "Sent MODULE_CPU and cpu_id = %d to Memory Stick", cpu_id);

	t_client_info *mem_stick = add_client_to_list(list_memory_stick, memory_stick_fd, credentials->id);

	log_info(logger, "Memory Stick %d connected (total: %d)", credentials->id, list_size(list_memory_stick));

	pthread_t thread;
	pthread_create(&thread, NULL, memory_stick_handler, mem_stick);
	pthread_detach(thread);

	return EXIT_SUCCESS;
}

void *memory_stick_handler (void *mem_stick_ptr) {
	t_client_info *mem_stick = (t_client_info *)mem_stick_ptr;
	log_debug(logger, "Memory Stick handler started for fd: %d, id: %d", mem_stick->fd, mem_stick->id);

	while (1) {
		//Handle connection with Memory Stick
		int op = operation_receive(mem_stick->fd);
        if (op == -1) {
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
