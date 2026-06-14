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

			default:{
				log_warning(logger, "Operación desconocida recibida: %d", op);
				break;
			}
		}
	}
	return NULL;
}
