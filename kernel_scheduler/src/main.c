#include <main.h>

t_log *logger;

t_scheduler_algorithm scheduler_algorithm;

t_list *list_cpu = NULL;
t_list *list_io = NULL;
t_list *list_processes = NULL;

int kernel_memory_fd = -1;

uint32_t next_cpu_id = 0;
uint32_t next_io_id = 0;

uint32_t current_max_pid = 0;

int main(int argc, char *argv[]) {
	/*-------------------Initial Setup-------------------*/
	if (argc < 3) {
        printf("Mode of use: %s <config_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

	t_config *config = config_create(argv[1]);
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);

	scheduler_algorithm = scheduler_algorithm_from_string(config_get_string_value(config, "PLANIFICATION_ALGORITHM"));

	list_cpu = list_create();
    list_io = list_create();
	list_processes = list_create();

	/*-------------------Connection with Kernel Memory-------------------*/
	if(kernel_memory_connection(logger, config, argv[2]) == EXIT_FAILURE) return EXIT_FAILURE;

	/*-------------------Server setup-------------------*/
	char *port = config_get_string_value(config, "KERNEL_SCHEDULER_PORT");
	
	int server_fd = server_start(port, logger);

	free(port);

	if (server_fd == -1) {
		log_error(logger, "Couldnt start server");
		return EXIT_FAILURE;
	}

	log_info(logger, "Kernel Scheduler ready, waiting for connections...");

	/*-------------------Handle connections-------------------*/
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

	log_destroy(logger);
    config_destroy(config);
	return EXIT_SUCCESS;
}

int kernel_memory_connection (t_log *logger, t_config *config, char *process0) {
	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	log_debug(logger, "Attempting connection with ip: %s, port: %s", kernel_memory_ip, kernel_memory_port);
	kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	if (kernel_memory_fd == -1) {
		log_error(logger, "Couldnt connect with Kernel Memory");
		return EXIT_FAILURE;
	}

	t_module_id_send (kernel_memory_fd, MODULE_KERNEL_SCHEDULER, logger);

	log_info(logger, "Connection successful with Kernel Memory");

	long_term_scheduler(process0, 0);

	pthread_t thread;
	pthread_create(&thread, NULL, kernel_memory_handler, NULL);
	pthread_detach(thread);

	free(kernel_memory_ip);
	free(kernel_memory_port);
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

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("log.log", "Kernel_Scheduler", 1, level);
	return logger;
}
