#include "kernel_memory_handler.h"

void *kernel_memory_handler (void *arg) {
	while (1) {
		//Handle connection with Kernel Memory
		int op = operation_receive(kernel_memory_fd);
		if (op == -1) {
			log_warning(logger, "Kernel Memory desconectado");
			close(kernel_memory_fd);
			exit(EXIT_FAILURE);
		}
	}
	return NULL;
}
