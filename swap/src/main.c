#include <main.h>

t_log *logger;
t_client_info *kernel_memory = NULL;

int main(void) {
    /*-------------------Connection with Kernel Memory-------------------*/
	
	t_config *config = config_create("swap.config");
    if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);	

	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

    log_info(logger, "SWAP started");

    int kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

    kernel_memory = create_client_info(kernel_memory_fd, 0);

    if(kernel_memory_fd == -1)
    {
        log_error(logger, "Conexion con Kernel Memory fallida");
        return EXIT_FAILURE;
    }

    t_module_id_send(kernel_memory->fd, MODULE_SWAP, logger, &kernel_memory->network_mutex);
    log_info(logger, "## Conectado a Kernel Memory");

    while (1) {
		int op = operation_receive(kernel_memory->fd);
        if (op == -1) {
            log_warning(logger, "Kernel Memory desconectado");
			destroy_client(kernel_memory);
            break;
        }
	}

    log_destroy(logger);
    config_destroy(config);
	return EXIT_SUCCESS;
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("swap.log", "SWAP", true, level);
	return logger;
}
