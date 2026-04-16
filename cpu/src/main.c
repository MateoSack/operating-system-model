#include <main.h>

t_log *logger;

int main(void)
{
	/*-------------------Connection with Kernel Scheduler-------------------*/
	uint32_t cpu_id;
	int kernel_scheduler_fd;

	t_config *config = config_create("cpu.config");
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);	

	char *kernel_scheduler_ip = config_get_string_value(config, "KERNEL_SCHEDULER_IP");
	char *kernel_scheduler_port = config_get_string_value(config, "KERNEL_SCHEDULER_PORT");

	log_info(logger, "CPU started");
	log_debug(logger, "Attempting connection with %s:%s", kernel_scheduler_ip, kernel_scheduler_port);
	kernel_scheduler_fd = connection_create(kernel_scheduler_ip, kernel_scheduler_port, logger);

	if (kernel_scheduler_fd == -1)
	{
		log_error(logger, "Connection attempt to Kernel scheduler failed.");
		return EXIT_FAILURE;
	}

	log_debug(logger, "Attempting to send t_module_id to %d", kernel_scheduler_fd);
	t_module_id_send(kernel_scheduler_fd, MODULE_CPU, logger);

	cpu_id = id_receive(kernel_scheduler_fd);
	log_info(logger, "Connection successful to Kernel Scheduler, CPU ID: %d", cpu_id);
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
