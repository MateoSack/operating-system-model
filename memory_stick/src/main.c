#include <main.h>


t_log *logger;
t_list *list_cpu = NULL;
uint32_t mem_stick_id;
t_client_info *kernel_memory = NULL;
uint32_t size = 0;
void *memory = NULL;

int main(int argc, char *argv[]) {
	/*-------------------Initial Setup-------------------*/
    if (argc < 3) {
        printf("Mode of use: ./bin/memory_stick <config_file> <size>\n");
        return EXIT_FAILURE;
    }

    t_config *config = config_create(argv[1]);
    if(config == NULL) return EXIT_FAILURE;
    logger = start_logger(config);

	size = atoi(argv[2]);
    if (size == 0) {
        fprintf(stderr, "Tamaño inválido: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    // Reservar la memoria que representa este memory stick
    memory = malloc(size);
    if (memory == NULL) {
        fprintf(stderr, "Error: no se pudo reservar %u bytes\n", size);
        return EXIT_FAILURE;
    }
    memset(memory, 0, size);

	list_cpu = list_create();

	/*-------------------Connection with Kernel Memory-------------------*/
	if (kernel_memory_handler(logger, config) == EXIT_FAILURE) {
        free(memory);
        return EXIT_FAILURE;
    }
	/*-------------------Server setup-------------------*/
	int server_fd = server_setup(logger, kernel_memory->fd);

	if (server_fd == -1) {
        log_error(logger, "No se pudo iniciar el servidor");
        free(memory);
        return EXIT_FAILURE;
    }

	log_info(logger, "Memory Stick listo, esperando conexiones...");

	/*-------------------Handle connections-------------------*/
	while(1) {
		int new_client_fd = server_client_wait(server_fd);

		if (new_client_fd == -1) {
            log_error(logger, "No se pudo aceptar la conexión");
            continue;
        }

		log_info(logger, "Nuevo cliente conectado: %d", new_client_fd);

        int *fd_for_thread = malloc(sizeof(int));
        *fd_for_thread = new_client_fd;

        pthread_t thread;
        pthread_create(&thread, NULL, cpu_handler, (void*)fd_for_thread);
        pthread_detach(thread);
	}

	free(memory);
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

	message_send(ip, kernel_memory_fd, &kernel_memory->network_mutex);
	message_send(port, kernel_memory_fd, &kernel_memory->network_mutex);
	free(ip);
	free(port);

	return(server_fd);
}

int kernel_memory_handler (t_log *logger, t_config *config) {
	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	log_debug(logger, "Attempting connection with ip: %s, port: %s", kernel_memory_ip, kernel_memory_port);
	kernel_memory = create_client_info(connection_create(kernel_memory_ip, kernel_memory_port, logger), 0);

	if (kernel_memory->fd == -1) {
		log_error(logger, "No se pudo conectar con Kernel Memory");
		return EXIT_FAILURE;
	}

	t_module_id_send(kernel_memory->fd, MODULE_MEMORY_STICK, logger, &kernel_memory->network_mutex);
	mem_stick_id = uint32_receive(kernel_memory->fd);
	uint32_send(kernel_memory->fd, size, &kernel_memory->network_mutex);
	
	log_info(logger, "## Conectado a Kernel Memory");
	log_info(logger, "MEMORY STICK ID: %d", mem_stick_id);

	pthread_t thread;
	pthread_create(&thread, NULL, kernel_memory_thread, NULL);
	pthread_detach(thread);

	free(kernel_memory_ip);
	free(kernel_memory_port);
	return EXIT_SUCCESS;
}

void *kernel_memory_thread(void *arg) {
    while (1) {
        int op = operation_receive(kernel_memory->fd);
        if (op == -1) {
            log_warning(logger, "Kernel Memory desconectado");
            destroy_client(kernel_memory);
            kernel_memory = NULL;
            exit(EXIT_FAILURE);
        }

        switch (op) {
            case MS_READ:
                handle_read(kernel_memory->fd, &kernel_memory->network_mutex);
                break;
            case MS_WRITE:
                handle_write(kernel_memory->fd, &kernel_memory->network_mutex);
                break;
            default:
                log_error(logger, "KM: operación desconocida: %d", op);
                break;
        }
    }
    return NULL;
}

void *cpu_handler (void *fd_ptr) {
	int cpu_fd = *(int *)fd_ptr; //trato fd_ptr como puntero a int y obtengo el valor para cpu_fd
	free(fd_ptr);

	t_module_id module_id = handshake_receiver(cpu_fd);
	log_debug(logger, "Se recibió HANDSHAKE de CPU, module_id: %d", module_id);
	if (module_id != MODULE_CPU) {
		log_error(logger, "Modulo desconocido conectado, t_module_id: %d", module_id);
		return NULL;
	}

	uint32_t id = uint32_receive(cpu_fd);

	t_client_info *cpu = add_client_to_list(list_cpu, cpu_fd, id);
	log_info(logger, "## CPU %d Conectada", id);
	log_info(logger, "Total de CPUs conectadas: %d", list_size(list_cpu));

	while (1) {
		//Handle connection with CPU
		int op = operation_receive(cpu_fd);
		if (op == -1) {
			log_warning(logger, "CPU %d desconectada", id);
            remove_client_from_list(list_cpu, cpu);
            destroy_client(cpu);
            break;
		}

		switch (op) {
			case MS_WRITE:
				handle_write(cpu_fd, &cpu->network_mutex);
				break;
			case MS_READ:
				handle_read(cpu_fd, &cpu->network_mutex);
				break;
			default:
				log_error(logger, "Operación desconocida: %d", op);
				break;
		}
	}
	return NULL;
}

void handle_write(int fd, pthread_mutex_t *net_mutex) {
    int total_size;
    int offset = 0;
    void *buffer = buffer_receive(&total_size, fd);
    if (buffer == NULL) {
        log_error(logger, "handle_write: no se recibieron datos");

        t_package *pkg = package_create();
        pkg->op_code = MS_WRITE_RESPONSE;
        package_add(pkg, true, sizeof(bool));
        package_send(pkg, fd, net_mutex);
        package_delete(pkg);
        return;
    }

    uint32_t local_offset = uint32_deserialize(buffer, &offset);
    uint32_t write_size   = uint32_deserialize(buffer, &offset);

    int data_field_size;
    memcpy(&data_field_size, buffer + offset, sizeof(int));
    offset += sizeof(int);
    void *data = buffer + offset;

    if (local_offset + write_size > size) { // Acá implementar escribir hasta donde se pueda y avisar al cliente que no se escribió todo
        log_error(logger, "handle_write: escritura fuera de rango (offset=%u size=%u total=%u)",
                  local_offset, write_size, size);
        free(buffer);

        t_package *pkg = package_create();
        pkg->op_code = MS_WRITE_RESPONSE;
        package_add(pkg, true, sizeof(bool));
        package_send(pkg, fd, net_mutex);
        package_delete(pkg);
        return;
    }

    memcpy(memory + local_offset, data, write_size);
    free(buffer);

    log_info(logger, "## Escritura de %u bytes", write_size);

    t_package *pkg = package_create();
    pkg->op_code = MS_WRITE_RESPONSE;
    package_add(pkg, true, sizeof(bool));
    package_send(pkg, fd, net_mutex);
    package_delete(pkg);
}

void handle_read(int fd, pthread_mutex_t *net_mutex) {
    int total_size;
    int offset = 0;
    void *buffer = buffer_receive(&total_size, fd);
    if (buffer == NULL) {
        log_error(logger, "handle_read: no se recibieron datos");
        return;
    }

    uint32_t local_offset = uint32_deserialize(buffer, &offset);
    uint32_t read_size    = uint32_deserialize(buffer, &offset);
    free(buffer);

    if (local_offset + read_size > size) {
        log_error(logger, "handle_read: lectura fuera de rango (offset=%u size=%u total=%u)",
                  local_offset, read_size, size);
        return;
    }

    log_info(logger, "## Lectura de %u bytes", read_size);

    t_package *pkg = package_create();
    pkg->op_code = MS_READ_RESPONSE;
    package_add(pkg, memory + local_offset, read_size);
    package_send(pkg, fd, net_mutex);
    package_delete(pkg);
}
