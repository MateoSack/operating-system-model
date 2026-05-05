#include "cpu_handler.h"

void cpu_handler (int cpu_fd) {
	uint32_t id = next_cpu_id;
	uint32_send(cpu_fd, id);
	next_cpu_id++;
	
	t_client_info *cpu = malloc(sizeof(t_client_info));
	cpu = add_client_to_list(list_cpu, cpu_fd, id);
	log_info(logger, "CPU %d connected (total: %d)", id, list_size(list_cpu));

	short_term_scheduler();

	while (1) {
		//Handle connection with CPU
		int op = operation_receive(cpu_fd);
        if (op == -1) {
            handle_cpu_disconnection(cpu);
            break;
        }

		switch (op) {
        	case PROCESS_CREATE:
            	uint32_t priority = uint32_decode (cpu->fd);
            	char *path = message_receive(logger, cpu->fd);
            
            	long_term_scheduler(path, priority);
            
            	break;
			
			case PROCESS_END:
				uint32_t pid = uint32_decode(cpu->fd);

				pthread_mutex_lock(&scheduler_mutex);

				t_process *process = get_process_from_pid(pid);

				if (process != NULL) {
					process_set_state(process, EXIT, logger);
					process_set_cpu(process, NULL);
				}
				cpu->is_available = true;

				pthread_mutex_unlock(&scheduler_mutex);

				log_info(logger, "## (%d) Process finished - Motive: EXIT", pid);

				short_term_scheduler();
				break;
        }
	}
}

void handle_cpu_disconnection (t_client_info *cpu) {
	int cpu_id = cpu->id;

	log_warning(logger, "CPU %d disconnected", cpu_id);
	close(cpu->fd);

	pthread_mutex_lock(&scheduler_mutex);
	t_process *process = get_process_by_cpu(cpu);
	bool had_process = (process != NULL);
	uint32_t pid = 0;

	if (had_process) {
		process_set_state(process, READY, logger);
		process_set_cpu(process, NULL);
		add_process_to_list(ready_queue, process);
		pid = process->pid;
	}
	remove_client_from_list(list_cpu, cpu);
	
	pthread_mutex_unlock(&scheduler_mutex);
	
	free(cpu);
	
	if (had_process) log_warning(logger, "CPU %d was executing process %d. Returning it to READY state.", cpu_id, pid);

	short_term_scheduler();
}
