#include <main.h>

t_log *logger;
t_list *list_cpu = NULL;
uint32_t mem_stick_id;
int kernel_memory_fd;

int main(void) {
	list_cpu = list_create();
	
	t_config *config = config_create("mem_stick.config");
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);

	/*-------------------Connection with Kernel Memory-------------------*/
	if(kernel_memory_handler(logger, config) == EXIT_FAILURE) return EXIT_FAILURE;

	/*-------------------Server setup-------------------*/
	int server_fd = server_setup(logger, kernel_memory_fd);

	if (server_fd == -1) {
		log_error(logger, "Couldnt start server");
		return EXIT_FAILURE;
	}

	log_info(logger, "Memory Stick ready, waiting for connections...");

	/*-------------------Handle connections-------------------*/
	while(1) {
		int new_client_fd = server_client_wait(server_fd);

		if (new_client_fd == -1) {
            log_error(logger, "Couldnt accept connection");
            continue;
        }

		log_info(logger, "New client connected: %d", new_client_fd);

        int *fd_for_thread = malloc(sizeof(int));
        *fd_for_thread = new_client_fd;

        pthread_t thread;
        pthread_create(&thread, NULL, cpu_handler, (void*)fd_for_thread);
        pthread_detach(thread);
	}

	log_destroy(logger);
    config_destroy(config);
	return EXIT_SUCCESS;
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("mem_stick.log", "MEMORY_STICK", true, level);
	return logger;
}

int server_setup (t_log *logger, int kernel_memory_fd) {
	int server_fd = server_start("0", logger);
	char *ip = get_ip_from_fd(kernel_memory_fd, logger);
	char *port = get_port_from_fd(server_fd, logger);

	message_send(ip, kernel_memory_fd);
	message_send(port, kernel_memory_fd);
	free(ip);
	free(port);

	return(server_fd);
}

int kernel_memory_handler (t_log *logger, t_config *config) {
	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	log_debug(logger, "Attempting connection with ip: %s, port: %s", kernel_memory_ip, kernel_memory_port);
	kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	if (kernel_memory_fd == -1) {
		log_error(logger, "Couldnt connect with Kernel Memory");
		return EXIT_FAILURE;
	}

	t_module_id_send(kernel_memory_fd, MODULE_MEMORY_STICK, logger);
	mem_stick_id = uint32_receive(kernel_memory_fd);
	log_info(logger, "Connection successful with Kernel Memory, MEMORY STICK ID: %d", mem_stick_id);

	pthread_t thread;
	pthread_create(&thread, NULL, kernel_memory_thread, NULL);
	pthread_detach(thread);

	free(kernel_memory_ip);
	free(kernel_memory_port);
	return EXIT_SUCCESS;
}

void *kernel_memory_thread () {
	while (1) {
		//Handle connection with Kernel Memory
		int op = operation_receive(kernel_memory_fd);
		if (op == -1) {
			log_warning(logger, "Kernel Memory disconnected");
			close(kernel_memory_fd);
			break;
		}
	}
	return NULL;
}

void *cpu_handler (void *fd_ptr) {
	int cpu_fd = *(int *)fd_ptr; //trato fd_ptr como puntero a int y obtengo el valor para cpu_fd
	free(fd_ptr);

	t_module_id module_id = handshake_receiver(cpu_fd);
	log_debug(logger, "Received handshake from CPU, module_id: %d", module_id);
	if (module_id != MODULE_CPU) {
		log_error(logger, "Unknown module connected, t_module_id: %d", module_id);
		return NULL;
	}

	uint32_t id = uint32_receive(cpu_fd);
	log_debug(logger, "Received cpu_id: %d", id);

	add_client_to_list(list_cpu, cpu_fd, id);
	log_info(logger, "CPU %d connected (total: %d)", id, list_size(list_cpu));

	t_client_info *cpu = malloc(sizeof(t_client_info));
	cpu->fd = cpu_fd;
	cpu->id = id;

	while (1) {
		//Handle connection with CPU
		int op = operation_receive(cpu_fd);
        if (op == -1) {
            log_warning(logger, "CPU %d disconnected", id);
			close(cpu_fd);
			remove_client_from_list(list_cpu, cpu);
			free(cpu);
            break;
        }
	}
	return NULL;
}
