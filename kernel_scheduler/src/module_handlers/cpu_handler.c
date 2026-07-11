#include "cpu_handler.h"

void cpu_handler (int cpu_fd) {
	uint32_t id = uint32_receive(cpu_fd);

	t_client_info *cpu = add_client_to_list(list_cpu, cpu_fd, id);
	log_info(logger, "## CPU %d Conectada", id);

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

				log_info(logger, "## (%d) - Solicitó syscall: INIT_PROC", pid);
				log_debug(logger, "Parametros: Priority: %d, Path: %s", priority, path);

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

				pthread_mutex_lock(&cpu->internal_mutex);
				cpu->is_available = true;
				pthread_mutex_unlock(&cpu->internal_mutex);

				pthread_mutex_unlock(&scheduler_mutex);

				uint32_send_with_op_code(kernel_memory->fd, pid, PROCESS_END, &kernel_memory->network_mutex);

				log_info(logger, "## (%d) finalizo su ejecución con motivo de EXIT", pid);

				sem_post(&short_term_scheduler_sem);
				break;
			}

			case MUTEX_CREATE : {
				uint32_t pid;
				char *mutex_name;

				receive_pid_and_name(&pid, &mutex_name, cpu->fd);

				t_process *process = get_process_from_pid(pid);

				log_info(logger, "## (%d) - Solicitó syscall: MUTEX_CREATE", process->pid);
				log_debug(logger, "Parametros: Nombre del mutex: %s", mutex_name);

				mutex_create(mutex_name);

				free(mutex_name);

				send_pid_to_execute(process->pid, cpu);
				break;
			}

			case MUTEX_LOCK : {
				uint32_t pid;
				char *mutex_name;

				receive_pid_and_name(&pid, &mutex_name, cpu->fd);

				t_process *process = get_process_from_pid(pid);

				if (process == NULL) {
					log_error(logger, "Sin procesos asociados a la CPU");
    				free(mutex_name);
    				break;
				}

				log_info(logger, "## (%d) - Solicitó syscall: MUTEX_LOCK", process->pid);
				log_debug(logger, "Parametros: Nombre del mutex: %s", mutex_name);
				
				t_mutex *mutex = get_mutex_by_name(mutex_name);

				if (mutex != NULL) {
					mutex_lock(mutex, process);
				} else {
					log_warning(logger, "Mutex '%s' no encontrado. Terminando proceso", mutex_name);
					pthread_mutex_lock(&cpu->internal_mutex);
					cpu->is_available = true;
					pthread_mutex_unlock(&cpu->internal_mutex);

					pthread_mutex_lock(&scheduler_mutex);
					remove_process_from_list(exec_processes, process);
					process_set_state(process, EXIT, logger);
					pthread_mutex_unlock(&scheduler_mutex);

					uint32_send_with_op_code(kernel_memory->fd, process->pid, PROCESS_END, &kernel_memory->network_mutex);

					sem_post(&short_term_scheduler_sem);
				}

				free(mutex_name);
				break;
			}

			case MUTEX_UNLOCK : {
				uint32_t pid;
				char *mutex_name;

				receive_pid_and_name(&pid, &mutex_name, cpu->fd);

				t_process *process = get_process_from_pid(pid);

				if (process == NULL) {
    				log_error(logger, "Sin procesos asociados a la CPU");
    				free(mutex_name);
    				break;
				}

				log_info(logger, "## (%d) - Solicitó syscall: MUTEX_UNLOCK", process->pid);
				log_debug(logger, "Parametros: Nombre del mutex: %s", mutex_name);

				t_mutex *mutex = get_mutex_by_name(mutex_name);

				if (mutex != NULL) {
					mutex_unlock(mutex, process);
				} else {
					log_warning(logger, "Mutex '%s' no encontrado. Terminando proceso", mutex_name);
					pthread_mutex_lock(&cpu->internal_mutex);
					cpu->is_available = true;
					pthread_mutex_unlock(&cpu->internal_mutex);

					pthread_mutex_lock(&scheduler_mutex);
					remove_process_from_list(exec_processes, process);
					process_set_state(process, EXIT, logger);
					pthread_mutex_unlock(&scheduler_mutex);

					uint32_send_with_op_code(kernel_memory->fd, process->pid, PROCESS_END, &kernel_memory->network_mutex);

					sem_post(&short_term_scheduler_sem);
				}

				free(mutex_name);
				break;
			}

			case SLEEP : {
				uint32_t pid;
				uint32_t sleep_time;

				receive_instruction_sleep(&pid, &sleep_time, cpu->fd);

				t_process *process = get_process_from_pid(pid);

				log_info(logger, "## (%d) - Solicitó syscall: SLEEP", pid);
				log_debug(logger, "Parametros: Tiempo de sleep: %d ms", sleep_time);

				if (process != NULL) {
					sleep_syscall_manager(process, cpu, sleep_time);
				} else {
					log_error(logger, "Proceso con PID %d no encontrado para ejecutar SLEEP", pid);
				}

				break;
			}

			case STDIN : {
				uint32_t pid;
				uint32_t physical_address;
				uint32_t to_read;

				receive_instruction_std(&pid, &physical_address, &to_read, cpu->fd);

				t_process *process = get_process_from_pid(pid);

				log_info(logger, "## (%d) - Solicitó syscall: STDIN", pid);
				log_debug(logger, "Parametros: Dirección física: %d, Bytes a leer: %d", physical_address, to_read);

				if (process != NULL) {
					stdin_syscall_manager(process, cpu, physical_address, to_read);
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
				log_debug(logger, "Parametros: Base: %d, Límite: %d", base, limit);

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

				log_info(logger, "## PID %u - Solicitó syscall: MEM_ALLOC", pid);
				log_debug(logger, "Parametros: Segment ID: %u, Size: %u", segment_id, size);

				mem_alloc_syscall_manager(pid, segment_id, size);
				break;
			}

			case MEM_FREE: {
				uint32_t pid;
				uint32_t segment_id;

				receive_instruction_mem_free(&pid, &segment_id, cpu->fd);

				log_info(logger, "## PID %u - Solicitó syscall: MEM_FREE", pid);
				log_debug(logger, "Parametros: Segment ID: %u", segment_id);

				mem_free_syscall_manager(pid, segment_id);
				break;
			}

			case SEGMENTATION_FAULT: {
				uint32_t pid = uint32_decode(cpu->fd);

				pthread_mutex_lock(&scheduler_mutex);

				t_process *process = get_process_from_pid(pid);

				if (process != NULL) {
					process_set_state(process, EXIT, logger);
					process_set_cpu(process, NULL);
					remove_process_from_list(exec_processes, process);
				}

				cpu->is_available = true;

				pthread_mutex_unlock(&scheduler_mutex);

				uint32_send_with_op_code(kernel_memory->fd, pid, PROCESS_END, &kernel_memory->network_mutex);

				log_info(logger, "## (%d) finalizo su ejecución con motivo de SEGMENTATION_FAULT", pid);

				sem_post(&short_term_scheduler_sem);
				break;
			}

			case CONFIRMATION: { // Confirms a process was successfully evicted and is ready to be sent to the ready queue
				uint32_t pid = uint32_decode(cpu->fd);
				log_debug(logger, "PID %d - Confirmación recibida", pid);
				sem_post(&cpu->response_sem);
				log_debug(logger, "sem_post hecho para CPU %d", cpu->id);
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

void receive_pid_and_name (uint32_t *pid, char **name, int client_fd) {
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, client_fd);
    if (buffer == NULL) { *pid = 0; *name = NULL; return; }

    *pid  = uint32_deserialize(buffer, &offset);
    *name = string_deserialize(buffer, &offset);

    free(buffer);
}
