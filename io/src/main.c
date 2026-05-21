#include <main.h>
#include <unistd.h> // Necesario para usleep

t_log *logger;
uint32_t pid;
int main(void)
{


	/*-------------------Connection with Kernel Scheduler-------------------*/
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
		log_error(logger, "Connection attempt to Kernel scheduler failed.");
		return EXIT_FAILURE;
	}

	t_module_id_send(kernel_scheduler_fd, MODULE_IO, logger);
	io_id = uint32_receive(kernel_scheduler_fd);
	log_info(logger, "Connection successful with Kernel Scheduler, IO ID: %d", io_id);

	while (1) {
		int op = operation_receive(kernel_scheduler_fd);
        if (op == -1) {
            log_warning(logger, "Kernel Scheduler disconnected");
			close(kernel_scheduler_fd);
            break;
        }
		switch (op) {
			case IO_PROCESS://ESPERA 3 UINT32 PID, 
			{
				pid = uint32_decode(kernel_scheduler_fd);
				log_info(logger, "## PID:  %d - Inicio de IO", pid);

			}
		}
	}

	log_destroy(logger);
    config_destroy(config);
	return EXIT_SUCCESS;
}

char *io_stdin(uint32_t pid,uint32_t size){
	char input[size]= {0};
	log_info(logger, "## PID: %d - Ingrese %d caracteres:", pid, size);
	fgets(input, sizeof(input), stdin);
	input[strcspn(input, "\n")] = '\0';
	log_info(logger, "## PID:  %d - Fin de IO", pid);
	return input;
} 


void io_sleep_ms(uint32_t pid,uint32_t ms) {
	log_info(logger, "## PID: %d - Haciendo sleep por %d milisegundos.", pid, ms);
	usleep(ms*1000); 
	log_info(logger, "## PID:  %d - Fin de IO", pid);
	return;
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("io.log", "IO", true, level);
	return logger;
}
