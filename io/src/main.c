#include <main.h>

int main(void)
{
	char *ip;
	char *port;
	int io_id;
	int kernel_scheduler_fd;
	t_log *logger;
	t_package *pkg = package_create();
	t_config *config;
	config = config_create("io.config");
	ip = config_get_string_value(config, "IP");
	port = config_get_string_value(config, "PORT");

	logger = log_create("io.log", "IO", true, LOG_LEVEL_INFO);
	log_info(logger, "IO started");
	kernel_scheduler_fd = connection_create(ip, port, logger);

	if (kernel_scheduler_fd == -1)
	{
		log_error(logger, "kernelscheduler connection failed");
		return EXIT_FAILURE;
	}

	package_add(pkg, MODULE_IO, sizeof(int));
	package_send(pkg, kernel_scheduler_fd);
	package_delete(pkg);
	operation_receive(kernel_scheduler_fd);
	io_id= id_receive(kernel_scheduler_fd);
}