#include <main.h>

t_log *logger;

int kernel_scheduler_fd = -1;
int kernel_memory_fd = -1;
uint32_t cpu_id;

int main(void)
{
	/*-------------------Initial Setup-------------------*/
	t_config *config = config_create("cpu.config");
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);	
	log_info(logger, "CPU started");

	/*-------------------Connection with Kernel Scheduler-------------------*/
	if(connect_kernel_scheduler(logger, config, cpu_id) == EXIT_FAILURE) return EXIT_FAILURE;

	/*-------------------Connection with Kernel Memory-------------------*/
	if(connect_kernel_memory(logger, config, cpu_id) == EXIT_FAILURE) return EXIT_FAILURE;
}

int connect_kernel_memory (t_log *logger, t_config *config, uint32_t cpu_id) {
	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	log_debug(logger, "Attempting connection with ip: %s, port: %s", kernel_memory_ip, kernel_memory_port);
	kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	if (kernel_memory_fd == -1) {
		log_info(logger, "Couldnt connect with Kernel Memory");
		return EXIT_FAILURE;
	}

	t_module_id_send (kernel_memory_fd, MODULE_CPU, logger);

	log_info(logger, "Connection successful with Kernel Memory");

	pthread_t thread;
	pthread_create(&thread, NULL, kernel_memory_thread, NULL);
	pthread_detach(thread);

	free(kernel_memory_ip);
	free(kernel_memory_port);
	return EXIT_SUCCESS;
}

int connect_kernel_scheduler(t_log *logger, t_config *config, uint32_t cpu_id) {
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

	pthread_t thread;
	pthread_create(&thread, NULL, kernel_scheduler_thread, NULL);
	pthread_detach(thread);

	free(kernel_scheduler_ip);
	free(kernel_scheduler_port);
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

void *kernel_scheduler_thread () {
	while (1) {
		//Handle connection with Kernel Scheduler
		int op = operation_receive(kernel_scheduler_fd);
		if (op == -1) {
			log_warning(logger, "Kernel Scheduler disconnected");
			close(kernel_scheduler_fd);
			break;
		}
	}
	return NULL;
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("cpu.log", "CPU", true, level);
	return logger;
}

/*enviar_mensaje(value, connection);
manejar_flujo_pcbs(logger, connection);
end_program(logger, config);
*/
/*while (1) {
	int op_cod = operation_receive(kernel_scheduler_fd);
	switch (op_cod) {
	case MESSAGE:
		message_receive(conexion, logger);
		break;
	case PACKAGE:

		break;
	case -1:
		log_error(logger, "el servidor se desconecto. Terminando proceso");
		return 1;
	default:
		log_warning(logger,"Operacion desconocida. No quieras meter la pata");
		break;
	}
}
	*/
/*
void end_program(t_log *logger, t_config *config)
{
	log_destroy(logger);

	config_destroy(config);

}
*/
