#include <main.h>

t_log *logger;

int main(void)
{
	char *ip;
	char *port;
	int mem_stick_id;
	int kernel_memory_fd;
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

	t_module_id_send(kernel_memory_fd, MODULE_MEMORY_STICK, logger);
	mem_stick_id = id_receive(kernel_memory_fd);
	log_info(logger, "Connection successful with Kernel Memory, MEMORY STICK ID: %d", mem_stick_id);
}