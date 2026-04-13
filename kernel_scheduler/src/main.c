#include <main.h>

t_log *logger;

t_list *list_cpu = NULL;
t_list *list_io = NULL;

int kernel_memory_fd = -1;

int next_cpu_id = 0;
int next_io_id = 0;

int main(void) {
	//t_config *config = config_create("kernel_scheduler.config");
	t_log *logger = log_create("log.log", "Kernel_Scheduler", 1, LOG_LEVEL_DEBUG);

	list_cpu = list_create();
    list_io = list_create();

	char *kernel_memory_ip = "127.0.0.1"; //Hardcoded for now
	char *kernel_memory_port = "4444";

	//-------------Connection with Kernel Memory-----------
	kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	if (kernel_memory_fd == -1) {
		log_info(logger, "Couldnt connect with Kernel Memory");
		return EXIT_FAILURE;
	}

	first_connection_with_kernel_memory (kernel_memory_fd);

	log_info(logger, "Connection successful with Kernel Memory");
	
	int server_fd = server_start(logger);

	if (server_fd == -1)
	{
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

        int *fd_for_thread = malloc(sizeof(int));
        *fd_for_thread = new_client_fd;

        pthread_t thread;
        pthread_create(&thread, NULL, client_handler_selector, (void*)fd_for_thread);
        pthread_detach(thread);
	}

	return EXIT_SUCCESS;
}

void *client_handler_selector (void *fd_ptr) {
	int client_fd = *(int *)fd_ptr; //trato fd_ptr como puntero a int y obtengo el valor para fd_client
	free(fd_ptr);

	t_module_id module_id = handshake_receiver(client_fd);

	switch (module_id) {
		case MODULE_CPU:
			cpu_handler(client_fd);
			break;

		case MODULE_IO:
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
	package_add(pkg, MODULE_KERNEL_SCHEDULER, sizeof(t_module_id));
    package_send(pkg, kernel_memory_fd);
    package_delete(pkg);
}

t_module_id handshake_receiver (int fd_client) {
	int cod_op = operation_receive(fd_client);
    if (cod_op != HANDSHAKE) {
        log_error(logger, "Expected HANDSHAKE, received: %d", cod_op);
        close(fd_client);
        return -1;
    }

    int size;
	int offset = 0;
    void *buffer = buffer_receive(&size, fd_client);
    t_module_id module_id = t_module_id_deserialize(buffer, &offset);
    free(buffer);
	return module_id;
}

void cpu_handler (int cpu_fd) {
	int id = id_assigner(MODULE_CPU, cpu_fd);

	add_client_to_list(MODULE_CPU, cpu_fd, id);
	log_info(logger, "CPU %d connected (total: %d)", id, list_size(list_cpu));

	/*
	while (1) {
		//Handle connection with CPU
	}
	*/
}

void io_handler (int io_fd) {
	int id = id_assigner(MODULE_IO, io_fd);

	add_client_to_list(MODULE_IO, io_fd, id);
	log_info(logger, "IO %d connected (total: %d)", id, list_size(list_io));

	/*
	while (1) {
		//Handle connection with IO
	}
	*/
}

int id_assigner (t_module_id module_type, int client_fd) {
	t_package *pkg = package_create();
	switch (module_type) {
		case MODULE_CPU:
			package_add(pkg, next_cpu_id, sizeof(t_module_id));
			next_cpu_id++;
			break;

		case MODULE_IO:
			package_add(pkg, next_io_id, sizeof(t_module_id));
			next_io_id++;
			break;

		default:
			log_error(logger, "Unknown module")
	}
    package_send(pkg, client_fd);
    package_delete(pkg);
	return next_cpu_id - 1;
}

void add_client_to_list (t_module_id module_type, int client_fd, int id) {
	t_client_info *client = malloc(sizeof(t_client_info));
	client->fd = client_fd;
	client->id = id;

	switch (module_type) {
		case MODULE_CPU:
			list_add(list_cpu, cpu);
			break;

		case MODULE_IO:
			list_add(list_io, cpu);
			break;
	}
}