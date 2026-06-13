#include "kernel_memory_handler.h"

void *kernel_memory_handler (void *arg) {
	while (1) {
		//Handle connection with Kernel Memory
		int op = operation_receive(kernel_memory->fd);

		switch (op) {
			case -1:{
				log_error(logger, "Kernel Memory desconectado");
				destroy_client(kernel_memory);
				exit(EXIT_FAILURE);
				break;
			}

			case COMPACTION_REQUEST: {
				compaction_requested();
			}

			case COMPACTION_FINISHED: {
				sem_post(&compaction_finished_sem);
			}
			
			default:{
				log_warning(logger, "Operación desconocida recibida: %d", op);
				break;
			}
		}
	}
	return NULL;
}
