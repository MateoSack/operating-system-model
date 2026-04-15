#include <main.h>

t_log *logger;

int main(void)
{
	uint32_t io_id;
	int kernel_scheduler_fd;

	t_config *config = config_create("io.config");
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);	

	char *kernel_scheduler_ip = config_get_string_value(config, "KERNEL_SCHEDULER_IP");
	char *kernel_scheduler_port = config_get_string_value(config, "KERNEL_SCHEDULER_PORT");

	log_info(logger, "IO started");

	kernel_scheduler_fd = connection_create(kernel_scheduler_ip, kernel_scheduler_port, logger);

	if (kernel_scheduler_fd == -1)
	{
		log_error(logger, "kernelscheduler connection failed");
		return EXIT_FAILURE;
	}

	t_module_id_send(kernel_scheduler_fd, MODULE_IO, logger);
	io_id = id_receive(kernel_scheduler_fd);
	log_info(logger, "Connection successful with Kernel Scheduler, IO ID: %d", io_id);
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("io.log", "IO", true, level);
	return logger;
}
