#include <main.h>

t_log *logger;

int main(void)
{
	char *ip;
	char *port;
	uint32_t cpu_id;
	int kernel_scheduler_fd;
	t_config *config;
	config = config_create("cpu.config");
	ip = config_get_string_value(config, "IP");
	port = config_get_string_value(config, "PORT");

	logger = log_create("cpu.log", "CPU", true, LOG_LEVEL_INFO);
	log_info(logger, "CPU started");
	kernel_scheduler_fd = connection_create(ip, port, logger);

	if (kernel_scheduler_fd == -1)
	{
		log_error(logger, "kernelscheduler connection failed");
		return EXIT_FAILURE;
	}

	t_module_id_send(kernel_scheduler_fd, MODULE_CPU, logger);
	cpu_id = id_receive(kernel_scheduler_fd);
	log_info(logger, "Connection successful with Kernel Scheduler, CPU ID: %d", cpu_id);
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