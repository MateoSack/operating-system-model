#include "kernel_memory_handler.h"

void *kernel_memory_handler (void *arg) {
	while (1) {
		//Handle connection with Kernel Memory
		int op = operation_receive(kernel_memory->fd);
		if (op == -1) {
			log_error(logger, "Kernel Memory desconectado");
			destroy_client(kernel_memory);
			exit(EXIT_FAILURE);
		}
	}
	return NULL;
}
