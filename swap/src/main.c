#include <main.h>

t_log *logger;

int main(void) {
    /*-------------------Connection with Kernel Memory-------------------*/
	int kernel_memory_fd;
	
	t_config *config = config_create("swap.config");
    if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);	

	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

    log_info(logger, "SWAP started");

    kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

    if(kernel_memory_fd == -1)
    {
        log_error(logger, "Connection attempt with Kernel memory failed.");
        return EXIT_FAILURE;
    }

    t_module_id_send(kernel_memory_fd, MODULE_SWAP, logger);
    log_info(logger, "Successful connection to kernel memory");

    log_destroy(logger);
    config_destroy(config);
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("swap.log", "SWAP", true, level);
	return logger;
}
