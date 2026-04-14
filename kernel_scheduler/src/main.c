#include <main.h>

t_log *logger;

t_list *list_cpu = NULL;
t_list *list_io = NULL;

int kernel_memory_fd = -1;

int next_cpu_id = 0;
int next_io_id = 0;

int main(void) {
	t_config *config = config_create("kernel_scheduler.config");
	logger = start_logger(config);

	list_cpu = list_create();
    list_io = list_create();

	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	/*-------------------Connection with Kernel Memory-----------------------*/
	log_debug(logger, "Attempting connection with ip: %s, port: %s", kernel_memory_ip, kernel_memory_port);
	kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	if (kernel_memory_fd == -1) {
		log_info(logger, "Couldnt connect with Kernel Memory");
		return EXIT_FAILURE;
	}

	first_connection_with_kernel_memory (kernel_memory_fd);

	log_info(logger, "Connection successful with Kernel Memory");
	
	int server_fd = server_start(logger);

	if (server_fd == -1) {
		log_info(logger, "Couldnt start server");
		return EXIT_FAILURE;
	}

	log_info(logger, "Kernel Scheduler ready, waiting for connections...");

	while(1) {
		int new_client_fd = server_client_wait(server_fd);

		if (new_client_fd == -1) {
            log_error(logger, "Couldnt accept connection");
            continue;
        }

		log_debug(logger, "New client connected: %d", new_client_fd);

        int *fd_for_thread = malloc(sizeof(int));
        *fd_for_thread = new_client_fd;

        pthread_t thread;
        pthread_create(&thread, NULL, client_handler_selector, (void*)fd_for_thread);
        pthread_detach(thread);
	}

	return EXIT_SUCCESS;
}

void *client_handler_selector (void *fd_ptr) {
	int client_fd = *(int *)fd_ptr; //trato fd_ptr como puntero a int y obtengo el valor para client_fd
	free(fd_ptr);

	t_module_id module_id = handshake_receiver(client_fd);

	switch (module_id) {
		case MODULE_CPU:
			log_debug(logger, "New client is of type CPU");
			cpu_handler(client_fd);
			break;

		case MODULE_IO:
			log_debug(logger, "New client is of type IO");
			io_handler(client_fd);
			break;

		default:
            log_warning(logger, "Unknown module: %d", module_id);
            close(client_fd);
            return NULL;
	}

	return NULL;
}

void first_connection_with_kernel_memory (int kernel_memory_fd) {
	t_package *pkg = package_create();
	t_module_id module = MODULE_KERNEL_SCHEDULER;
	package_add(pkg, &module, sizeof(t_module_id));
    package_send(pkg, kernel_memory_fd);
    package_delete(pkg);
	log_debug(logger, "t_module_id sent to kernel_memory");
}

void cpu_handler (int cpu_fd) {
	int id = id_assigner(&next_cpu_id, cpu_fd);

	add_client_to_list(list_cpu, cpu_fd, id);
	log_info(logger, "CPU %d connected (total: %d)", id, list_size(list_cpu));

	/*
	while (1) {
		//Handle connection with CPU
	}
	*/
}

void io_handler (int io_fd) {
	int id = id_assigner(&next_io_id, io_fd);

	add_client_to_list(list_io, io_fd, id);
	log_info(logger, "IO %d connected (total: %d)", id, list_size(list_io));

	/*
	while (1) {
		//Handle connection with IO
	}
	*/
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("log.log", "Kernel_Scheduler", 1, level);
	return logger;
}
