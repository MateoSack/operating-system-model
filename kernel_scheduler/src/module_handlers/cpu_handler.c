#include "cpu_handler.h"

void cpu_handler (int cpu_fd) {
	uint32_t id = id_assigner(&next_cpu_id, cpu_fd, &cpu_id_mutex);
	
	t_client_info *cpu = add_client_to_list(list_cpu, cpu_fd, id);
	log_info(logger, "CPU %d conectado (total: %d)", id, list_size(list_cpu));

	sem_post(&short_term_scheduler_sem);

	while (1) {
		//Handle connection with CPU
		int op = operation_receive(cpu_fd);
        if (op == -1) {
            handle_cpu_disconnection(cpu);
            break;
        }

		switch (op) {
        	case PROCESS_CREATE: {
				uint32_t pid;
				uint32_t priority;
				char *path;
				
				receive_instruction_process_create(&pid, &priority, &path, cpu->fd);
				
				log_info(logger, "## (%d) - Solicitó syscall: INIT_PROC (Priority: %d, Path: %s)", pid, priority, path);
            
            	long_term_scheduler(path, priority);
            
            	break;
			}
			
			case PROCESS_END: {
				uint32_t pid = uint32_decode(cpu->fd);
				
				log_info(logger, "## (%d) - Solicitó syscall: EXIT", pid);

				pthread_mutex_lock(&scheduler_mutex);

				t_process *process = get_process_from_pid(pid);

				if (process != NULL) {
					process_set_state(process, EXIT, logger);
					process_set_cpu(process, NULL);
					remove_process_from_list(exec_processes, process);
				}

				cpu->is_available = true;

				pthread_mutex_unlock(&scheduler_mutex);

				send_pid_with_op_code(pid, PROCESS_END, kernel_memory->fd, &kernel_memory->network_mutex);

				log_info(logger, "## (%d) finalizo su ejecución con motivo de EXIT", pid);

				sem_post(&short_term_scheduler_sem);
				break;
			}

			case MUTEX_CREATE : {
				char *mutex_name = message_decode(cpu->fd);
				t_process *process = get_process_from_cpu(cpu);
				log_info(logger, "## (%d) - Solicitó syscall: MUTEX_CREATE (Nombre del mutex: %s)", process->pid, mutex_name);
				mutex_create(mutex_name);
				free(mutex_name);
				break;
			}

			case MUTEX_LOCK : {
				char *mutex_name = message_decode(cpu->fd);
				
				t_process *process = get_process_from_cpu(cpu);
				
				log_info(logger, "## (%d) - Solicitó syscall: MUTEX_LOCK (Nombre del mutex: %s)", process->pid, mutex_name);

				if (process == NULL) {
    				log_error(logger, "Sin procesos asociados a la CPU");
    				free(mutex_name);
    				break;
				}

				t_mutex *mutex = get_mutex_by_name(mutex_name);

				if (mutex != NULL) {
					mutex_lock(mutex, process);
				} else {
					log_warning(logger, "Mutex '%s' no encontrado", mutex_name);
				}

				free(mutex_name);
				break;
			}

			case MUTEX_UNLOCK : {
				char *mutex_name = message_decode(cpu->fd);
				
				t_process *process = get_process_from_cpu(cpu);
				
				log_info(logger, "## (%d) - Solicitó syscall: MUTEX_UNLOCK (Nombre del mutex: %s)", process->pid, mutex_name);
				
				if (process == NULL) {
    				log_error(logger, "Sin procesos asociados a la CPU");
    				free(mutex_name);
    				break;
				}
				
				t_mutex *mutex = get_mutex_by_name(mutex_name);

				if (mutex != NULL) {
					mutex_unlock(mutex, process);
				} else {
					log_warning(logger, "Mutex '%s' no encontrado", mutex_name);
				}

				free(mutex_name);
				break;
			}

			case SLEEP : {
				uint32_t pid;
				uint32_t sleep_time;
				
				receive_instruction_sleep(&pid, &sleep_time, cpu->fd);
				
				t_process *process = get_process_from_pid(pid);

				log_info(logger, "## (%d) - Solicitó syscall: SLEEP (Tiempo: %d ms)", pid, sleep_time);

				if (process != NULL) {
					sleep_syscall_manager(process, cpu, sleep_time);
				} else {
					log_error(logger, "Proceso con PID %d no encontrado para ejecutar SLEEP", pid);
				}

				break;
			}

			case STDIN : {
				uint32_t pid;
				uint32_t base;
				uint32_t limit;
				
				receive_instruction_std(&pid, &base, &limit, cpu->fd);
				
				t_process *process = get_process_from_pid(pid);
				
				log_info(logger, "## (%d) - Solicitó syscall: STDIN (Base: %d, Limit: %d)", pid, base, limit);

				if (process != NULL) {
					stdin_syscall_manager(process, cpu, base, limit);
				} else {
					log_error(logger, "Proceso con PID %d no encontrado para ejecutar STDIN", pid);
				}

				break;
			}

			case STDOUT : {
				uint32_t pid;
				uint32_t base;
				uint32_t limit;
				
				receive_instruction_std(&pid, &base, &limit, cpu->fd);
				
				t_process *process = get_process_from_pid(pid);
				
				log_info(logger, "## (%d) - Solicitó syscall: STDOUT", pid);

				if (process != NULL) {
					stdout_syscall_manager(process, cpu, base, limit);
				} else {
					log_error(logger, "Proceso con PID %d no encontrado para ejecutar STDOUT", pid);
				}

				break;
			}

			case PROCESS_INTERRUPTED: {
				uint32_t pid;
				t_interrupt_reason reason;

				receive_interruption(&pid, &reason, cpu->fd);

				log_info(logger, "## PID %d finalizó su ejecución con motivo de %s", pid, interrupt_reason_to_string(reason));
				break;
			}

			case MEM_ALLOC: {
				uint32_t pid;
				uint32_t segment_id;
				uint32_t size;

				receive_instruction_mem_alloc(&pid, &segment_id, &size, cpu->fd);

				mem_alloc_syscall_manager(pid, segment_id, size);

				log_info(logger, "## PID %u - Solicitó syscall: MEM_ALLOC (Segment ID: %u, Size: %u)", pid, segment_id, size);
				break;
			}

			case MEM_FREE: {
				uint32_t pid;
				uint32_t segment_id;

				receive_instruction_mem_free(&pid, &segment_id, cpu->fd);

				mem_free_syscall_manager(pid, segment_id);

				log_info(logger, "## PID %u - Solicitó syscall: MEM_FREE (Segment ID: %u)", pid, segment_id);
				break;
			}

			case CONFIRMATION: { // Confirms a process was successfully evicted and is ready to be sent to the ready queue
				uint32_t pid = uint32_decode(cpu->fd);
				log_info(logger, "## PID %d - Confirmación recibida", pid);
				sem_post(&cpu->response_sem);
				break;
			}
        }
	}
}

void handle_cpu_disconnection (t_client_info *cpu) {
	int cpu_id = cpu->id;

	log_warning(logger, "CPU %d desconectada", cpu_id);
	close(cpu->fd);

	pthread_mutex_lock(&scheduler_mutex);
	t_process *process = get_process_from_cpu(cpu);
	bool had_process = (process != NULL);
	uint32_t pid = 0;

	if (had_process) {
		process_set_state(process, READY, logger);
		process_set_cpu(process, NULL);
		add_process_to_ready_queue(process);
		remove_process_from_list(exec_processes, process);
		pid = process->pid;
		process->start_exec_time = 0;
	}

	remove_client_from_list(list_cpu, cpu);
	
	pthread_mutex_unlock(&scheduler_mutex);
	
	destroy_client(cpu);
	
	if (had_process) log_warning(logger, "CPU %d estaba ejecutando el proceso %d. Devolviéndolo al estado READY.", cpu_id, pid);

	sem_post(&short_term_scheduler_sem);
}

void receive_instruction_process_create (uint32_t *pid, uint32_t *priority, char **path, int cpu_fd) {
	int size;
	int offset = 0;
	void *buffer = buffer_receive(&size, cpu_fd);
	if (buffer == NULL) return;

	*pid = uint32_deserialize(buffer, &offset);
	*priority = uint32_deserialize(buffer, &offset);

	int path_size;
	memcpy(&path_size, buffer + offset, sizeof(int));
	offset += sizeof(int);
	*path = malloc(path_size);
	memcpy(*path, buffer + offset, path_size);
	offset += path_size;

	free(buffer);
}

void receive_interruption (uint32_t *pid, t_interrupt_reason *reason, int cpu_fd) {
	int size;
	int offset = 0;
	void *buffer = buffer_receive(&size, cpu_fd);
	if (buffer == NULL) return;

	*pid = uint32_deserialize(buffer, &offset);
	*reason = t_interrupt_reason_deserialize(buffer, &offset);

	free(buffer);
}

void send_pid_with_op_code (uint32_t pid, op_code op_code, int client_socket, pthread_mutex_t *mutex) {
	t_package *pkg = package_create();
	pkg->op_code = op_code;
	package_add(pkg, &pid, sizeof(uint32_t));
	package_send(pkg, client_socket, mutex);
	package_delete(pkg);
}
