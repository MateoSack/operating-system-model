#include <main.h>

int main(void)
{
	char *ip;
	char *port;
	int mem_stick_id;
	int kernel_memory_fd;
	t_log *logger;
	t_package *pkg = package_create();
	t_config *config;
	config = config_create("mem_stick.config");
	ip = config_get_string_value(config, "IP");
	port = config_get_string_value(config, "PORT");

	logger = log_create("mem_stick.log", "MEMORY_STICK", true, LOG_LEVEL_INFO);
	log_info(logger, "MEMORY STICK started");
	kernel_memory_fd = connection_create(ip, port, logger);

	if (kernel_memory_fd == -1)
	{
		log_error(logger, "kernelmemory connection failed");
		return EXIT_FAILURE;
	}

	package_add(pkg, MODULE_MEMORY_STICK, sizeof(int));
	package_send(pkg, kernel_memory_fd);
	package_delete(pkg);
	operation_receive(kernel_memory_fd);
	mem_stick_id= id_receive(kernel_memory_fd);
}