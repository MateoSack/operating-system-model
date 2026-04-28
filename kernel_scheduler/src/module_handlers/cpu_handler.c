#include "cpu_handler.h"

void cpu_handler (int cpu_fd) {
	uint32_t id = next_cpu_id;
	uint32_send(cpu_fd, id);
	next_cpu_id++;
	
	t_client_info *cpu = malloc(sizeof(t_client_info));
	cpu = add_client_to_list(list_cpu, cpu_fd, id);
	log_info(logger, "CPU %d connected (total: %d)", id, list_size(list_cpu));

	while (1) {
		//Handle connection with CPU
		int op = operation_receive(cpu_fd);
        if (op == -1) {
            log_warning(logger, "CPU %d disconnected", id);
			close(cpu_fd);
			remove_client_from_list(list_cpu, cpu);
			free(cpu);
            break;
        }

		switch (op) {
        	case PROCESS_CREATE:
            	uint32_t priority = uint32_decode (cpu->fd);
            	char *path = message_receive(logger, cpu->fd);
            
            	long_term_scheduler(path, priority);
            
            	break;
        }
	}
}
