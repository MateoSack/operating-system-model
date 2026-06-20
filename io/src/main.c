#include <main.h>
#include <unistd.h> // Necesario para usleep

t_log *logger;
uint32_t io_id;
t_client_info *kernel_scheduler = NULL;

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Modo de uso: %s <config_file> <Tipo>\n", argv[0]);
        return EXIT_FAILURE;
    }
	
	/*-------------------Connection with Kernel Scheduler-------------------*/
	
	t_config *config = config_create(argv[1]);
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);	
	
	t_io_type io_type;
	io_type = io_type_from_string(argv[2]);
	if (io_type == -1) {
		log_error(logger, "Tipo de IO desconocido: %s", argv[2]);
		return EXIT_FAILURE;
	}

	char *kernel_scheduler_ip = config_get_string_value(config, "KERNEL_SCHEDULER_IP");
	char *kernel_scheduler_port = config_get_string_value(config, "KERNEL_SCHEDULER_PORT");

	log_info(logger, "IO started");

	kernel_scheduler = create_client_info(connection_create(kernel_scheduler_ip, kernel_scheduler_port, logger), 0);

	if (kernel_scheduler->fd == -1) {
		log_error(logger, "Conexión con Kernel scheduler fallida");
		return EXIT_FAILURE;
	}

	t_module_id_send(kernel_scheduler->fd, MODULE_IO, logger, &kernel_scheduler->network_mutex);
	t_io_type_send(kernel_scheduler->fd, io_type, logger, &kernel_scheduler->network_mutex);
	io_id = uint32_receive(kernel_scheduler->fd);
	log_info(logger, "## Conectado a Kernel Scheduler");
	log_info(logger, "## IO ID asignada por Kernel Scheduler: %d", io_id);

	while (1) {
		int op = operation_receive(kernel_scheduler->fd);

		log_debug(logger, "Operacion recibida de Kernel Scheduler: %d", op);

		switch (op) {
			case -1: {
				log_error(logger, "Kernel Scheduler desconectado");
				destroy_client(kernel_scheduler);
				return EXIT_FAILURE;
			}

			default: {
				handle_operation(kernel_scheduler->fd, io_type);
			}
		}
	}

	log_destroy(logger);
    config_destroy(config);
	return EXIT_SUCCESS;
}

const t_io_type io_type_from_string(const char *str) {
	if (strcmp(str, "STDIN") == 0) return IO_TYPE_STDIN;
	if (strcmp(str, "STDOUT") == 0) return IO_TYPE_STDOUT;
	if (strcmp(str, "SLEEP") == 0) return IO_TYPE_SLEEP;
	return -1; // Unknown type
}

void handle_operation(int client_fd, t_io_type io_type) {
	switch (io_type) {
		case IO_TYPE_STDIN: {
			// Handle STDIN operation
			t_io_numeric_process *io_process = io_numeric_process_receive(client_fd);
			if (io_process != NULL) {
				log_info(logger, "## PID:  %d - Inicio de IO", io_process->pid);
				char *output = io_stdin(io_process->pid, io_process->value);
				t_io_string_process *string_process = t_io_string_process_create(io_process->pid, output, IO_TYPE_STDIN);
				io_string_process_send(string_process, STDIN, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
				free(io_process);
				free(output);
			}
			break;
		}

		case IO_TYPE_STDOUT: {
			// Handle STDOUT operation
			t_io_string_process *io_process = io_string_process_receive(client_fd);
			if (io_process != NULL) {
				log_info(logger, "## PID:  %d - Inicio de IO", io_process->pid);
				io_stdout(io_process->pid, io_process->value);
				send_confirmation(io_process->pid, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
				free(io_process->value);
				free(io_process);
			}
			break;
		}

		case IO_TYPE_SLEEP: {
			// Handle SLEEP operation
			t_io_numeric_process *io_process = io_numeric_process_receive(client_fd);
			if (io_process != NULL) {
				log_info(logger, "## PID:  %d - Inicio de IO", io_process->pid);
				io_sleep_ms(io_process->pid, io_process->value);
				send_confirmation(io_process->pid, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
				free(io_process);
			}
			break;
		}
		
		default:
			break;
	}
}

char *io_stdin(uint32_t pid, uint32_t size) {
    char *input = calloc(size + 1, sizeof(char));

    log_info(logger, "## PID: %d - Ingrese %d caracteres:", pid, size);

	char *read = readline("> ");

	int missing_characters = size -strlen(read);

	if (missing_characters > 0) {
		string_append(&input, read);
		for (int i = 0; i < missing_characters; i++) {
			string_append(&input, "\0");
		}
	} else if (missing_characters < 0) {
		string_n_append(&input, read, size);
	} else {
		string_append(&input, read);
	}

	free(read);

    log_info(logger, "## PID: %d - Fin de IO", pid);

    return input;
}

void io_stdout(uint32_t pid,char *output){
	log_info(logger, "## PID: %d - %s",pid, output);
	printf("%s\n", output);
	log_info(logger, "%s", output);
	return;
}

void io_sleep_ms(uint32_t pid, uint32_t ms) {
	log_info(logger, "## PID: %d - Haciendo sleep por %d milisegundos.", pid, ms);
	usleep(ms*1000); 
	log_info(logger, "## PID:  %d - Fin de IO", pid);
	return;
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("io.log", "IO", true, level);
	return logger;
}
