#include "kernel_memory_handler.h"

int kernel_memory_connection (t_log *logger, t_config *config, char *process0) { // Establishes connection with Kernel Memory and initializes process0
	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	log_debug(logger, "Intentando conexión con ip: %s, puerto: %s", kernel_memory_ip, kernel_memory_port);
	int kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	kernel_memory = create_client_info(kernel_memory_fd, 0);

	if (kernel_memory_fd == -1) {
		log_error(logger, "No se pudo conectar con Kernel Memory");
		return EXIT_FAILURE;
	}

	t_module_id_send (kernel_memory->fd, MODULE_KERNEL_SCHEDULER, logger, &kernel_memory->network_mutex);

	log_info(logger, "## Conectado a Kernel Memory");
	
	log_debug(logger, "Inicializando proceso0 con ruta: %s", process0);

	int init_result = long_term_scheduler(process0, 0);
	if (init_result == EXIT_SUCCESS) {
		log_debug(logger, "Proceso0 creado correctamente");
	} else {
		log_error(logger, "No se pudo crear proceso0");
	}

	pthread_t thread;
	pthread_create(&thread, NULL, kernel_memory_handler, NULL);
	pthread_detach(thread);

	free(kernel_memory_ip);
	free(kernel_memory_port);
	return EXIT_SUCCESS;
}

void *kernel_memory_handler (void *arg) {
	while (1) {
		//Handle connection with Kernel Memory
		int op = operation_receive(kernel_memory->fd);

		switch (op) {
			case -1:{
				log_error(logger, "Kernel Memory desconectado");
				destroy_client(kernel_memory);
				exit(EXIT_FAILURE);
				break;
			}

			case COMPACTION_REQUEST: {
				compaction_requested();
				break;
			}

			case COMPACTION_FINISHED: {
				sem_post(&compaction_finished_sem);
				break;
			}

			case CORRUPTED_MEMORY: {
				memory_corrupted();
				break;
			}

			case MEMORY_UPDATE: {
				uint32_t new_size = uint32_decode(kernel_memory->fd);

				memory_update(new_size);
				break;
			}

			case SEGMENT_RESULT: {
				uint32_t pid;
				uint32_t segment_id;
				t_segment_result result;

				receive_segment_result(&pid, &segment_id, &result);

				handle_segment_result(pid, segment_id, result);
				break;
			}

			case IO_MEMORY_READ: {
				uint32_t pid;
				char *value;
				receive_read_value_io_memory_read(&pid, &value);

				stdout_wait_memory_read(pid, value);
				break;
			}

			case IO_MEMORY_WRITE: {
				uint32_t pid;
				bool write_succesful;
				receive_confirmation_io_memory_write(&pid, &write_succesful);

				t_process *process = get_process_from_pid(pid);

				if (!write_succesful) {
					log_error(logger, "Hubo un error en la escritura de datos. Finalizando proceso...");
					process_set_state(process, EXIT, logger);

					uint32_send_with_op_code(kernel_memory->fd, process->pid, PROCESS_END, &kernel_memory->network_mutex);

					break;
				}

				sem_post(&process->memory_request_sem);

				break;
			}

			default:{
				log_warning(logger, "Operación desconocida recibida: %d", op);
				break;
			}
		}
	}

	return NULL;
}

void receive_confirmation_io_memory_write (uint32_t *pid, bool *write_succesful) {
    int size;
    int offset = 0;
	void *buffer = buffer_receive(&size, kernel_memory->fd);
	if (buffer == NULL) return;

	*pid = uint32_deserialize(buffer, &offset);
	*write_succesful = bool_deserialize(buffer, &offset);

	free(buffer);
}

void receive_read_value_io_memory_read (uint32_t *pid, char **value) {
    int size;
    int offset = 0;
	void *buffer = buffer_receive(&size, kernel_memory->fd);
	if (buffer == NULL) return;

	*pid = uint32_deserialize(buffer, &offset);
	*value = string_deserialize(buffer, &offset);

	free(buffer);
}

void receive_segment_result (uint32_t *pid, uint32_t *segment_id, t_segment_result *result) {
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, kernel_memory->fd);
    if (buffer == NULL) return;

    *pid = uint32_deserialize(buffer, &offset);
    *segment_id = uint32_deserialize(buffer, &offset);
    *result = segment_result_deserialize(buffer, &offset);

    free(buffer);
}
