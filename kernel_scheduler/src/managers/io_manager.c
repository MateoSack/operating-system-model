#include <managers/io_manager.h>

int sleep_syscall_manager (t_process *process, t_client_info *cpu, uint32_t sleep_time) { // Manages the sleep syscall for a process, sending it to an available IO device of type SLEEP
    pthread_mutex_lock(&scheduler_mutex);
    process_set_state(process, BLOCK, logger);
    process_set_cpu(process, NULL);
    remove_process_from_list(exec_processes, process);
    process->start_block_time = temporal_gettime(system_timer);
    pthread_mutex_lock(&block_processes_mutex);
    list_add(block_processes, process);
    pthread_mutex_unlock(&block_processes_mutex);
    int pid = process->pid;
    pthread_mutex_unlock(&scheduler_mutex);

    pthread_mutex_lock(&cpu->internal_mutex);
    cpu->is_available = true;
    pthread_mutex_unlock(&cpu->internal_mutex);

    t_io_numeric_process *io_process = t_io_numeric_process_create(pid, sleep_time, IO_TYPE_SLEEP);

    pthread_mutex_lock(&io_mutex);
    t_client_info *io = get_available_io_type(list_io_sleep);

    if (io != NULL) {
        pthread_mutex_lock(&io->internal_mutex);
        io->is_available = false;
        pthread_mutex_unlock(&io->internal_mutex);
        pthread_mutex_unlock(&io_mutex);

        io_numeric_process_send(io_process, SLEEP, io->fd, &io->network_mutex);

        log_debug(logger, "Proceso %d enviado a IO SLEEP (fd: %d) para dormir por %d ms", pid, io->fd, sleep_time);
    } else {
        list_add(pending_request_io_sleep, io_process);
        pthread_mutex_unlock(&io_mutex);

        log_debug(logger, "No hay dispositivos IO de tipo SLEEP disponibles para procesar la solicitud de sleep del proceso %d. El proceso quedará bloqueado hasta que un dispositivo IO de tipo SLEEP esté disponible.", pid);
    }

    sem_post(&short_term_scheduler_sem);

    return EXIT_SUCCESS;
}

int stdin_syscall_manager (t_process *process, t_client_info *cpu, uint32_t physical_address, uint32_t to_read) { // Manages the stdin syscall for a process, sending it to an available IO device of type STDIN
    pthread_mutex_lock(&scheduler_mutex);
    remove_process_from_list(exec_processes, process);
    int pid = process->pid;

    if (to_read == 0) {
        process_set_state(process, EXIT, logger);
        pthread_mutex_unlock(&scheduler_mutex);
        log_warning(logger, "Proceso %d pidio leer 0 bytes. Terminando proceso...", pid);
        uint32_send_with_op_code(kernel_memory->fd, process->pid, PROCESS_END, &kernel_memory->network_mutex);
        return EXIT_FAILURE;
    }

    process_set_state(process, BLOCK, logger);
    process_set_cpu(process, NULL);
    process->start_block_time = temporal_gettime(system_timer);
    pthread_mutex_lock(&block_processes_mutex);
    list_add(block_processes, process);
    pthread_mutex_unlock(&block_processes_mutex);

    pthread_mutex_unlock(&scheduler_mutex);

    pthread_mutex_lock(&cpu->internal_mutex);
    cpu->is_available = true;
    pthread_mutex_unlock(&cpu->internal_mutex);

    t_io_numeric_process *io_process = t_io_numeric_process_create(pid, to_read, IO_TYPE_STDIN);
    t_pending_stdin *pending_stdin = t_pending_stdin_create(pid, physical_address, to_read);

    pthread_mutex_lock(&pending_io_stdin_reading_mutex);
    list_add(pending_io_stdin_reading, pending_stdin);
    pthread_mutex_unlock(&pending_io_stdin_reading_mutex);

    pthread_mutex_lock(&io_mutex);
    t_client_info *io = get_available_io_type(list_io_stdin);

    if (io != NULL) {
        pthread_mutex_lock(&io->internal_mutex);
        io->is_available = false;
        pthread_mutex_unlock(&io->internal_mutex);
        pthread_mutex_unlock(&io_mutex);

        io_numeric_process_send(io_process, STDIN, io->fd, &io->network_mutex);

        log_debug(logger, "Proceso %d enviado a IO STDIN (fd: %d)", pid, io->fd);
    } else {
        list_add(pending_request_io_stdin, io_process);
        pthread_mutex_unlock(&io_mutex);

        log_debug(logger, "No hay dispositivos IO de tipo STDIN disponibles para procesar la solicitud de stdin del proceso %d. El proceso quedará bloqueado hasta que un dispositivo IO de tipo STDIN esté disponible.", pid);
    }

    sem_post(&short_term_scheduler_sem);

    return EXIT_SUCCESS;
}

void *wait_memory_write_confirmation (void *arg) {
    t_process *process = (t_process *) arg;

    sem_wait(&process->memory_request_sem);

    if (process != NULL) {
        pthread_mutex_lock(&scheduler_mutex);
        process_set_ready(process);
        if (process->state == READY) {
            add_process_to_ready_queue(process);
        }
        pthread_mutex_unlock(&scheduler_mutex);

        log_info(logger, "## PID: %d finalizó IO y pasa a READY / SUSP. READY", process->pid);

        sem_post(&short_term_scheduler_sem);
    } else {
        log_error(logger, "Proceso no encontrado");
    }

    return NULL;
}

t_pending_stdin *get_pending_stdin_from_pid (uint32_t pid) {
    bool _stdin_pid_coincides (void *ptr) {
        t_pending_stdin *p = (t_pending_stdin*)ptr;
        return p->pid == pid;
    }

    t_pending_stdin *pending_stdin = list_find(pending_io_stdin_reading, _stdin_pid_coincides);

    return pending_stdin;
}

t_pending_stdin *t_pending_stdin_create (uint32_t pid, uint32_t physical_address, uint32_t size) {
    t_pending_stdin *p = malloc(sizeof(t_pending_stdin));

    p->pid = pid;
    p->physical_address = physical_address;
    p->size = size;

    return p;
}

int stdout_syscall_manager (t_process *process, t_client_info *cpu, uint32_t physical_address, uint32_t to_read) { // Manages the stdout syscall for a process, sending it to an available IO device of type STDOUT
    pthread_mutex_lock(&scheduler_mutex);
    process_set_state(process, BLOCK, logger);
    process_set_cpu(process, NULL);
    remove_process_from_list(exec_processes, process);
    process->start_block_time = temporal_gettime(system_timer);
    pthread_mutex_lock(&block_processes_mutex);
    list_add(block_processes, process);
    pthread_mutex_unlock(&block_processes_mutex);
    int pid = process->pid;
    pthread_mutex_unlock(&scheduler_mutex);

    pthread_mutex_lock(&cpu->internal_mutex);
    cpu->is_available = true;
    pthread_mutex_unlock(&cpu->internal_mutex);

    send_memory_read(pid, physical_address, to_read);

    log_debug(logger, "Lectura de datos solicitada");

    return EXIT_SUCCESS;
}

void stdout_wait_memory_read (uint32_t pid, char *value) {
    if (strcmp(value, "") == 0) {
        log_warning(logger, "Hubo un error al leer los datos. Finalizando proceso...");
        process_set_state(get_process_from_pid(pid), EXIT, logger);
        uint32_send_with_op_code(kernel_memory->fd, pid, PROCESS_END, &kernel_memory->network_mutex);
    }

    log_debug(logger, "Datos leidos: %s", value);

    t_io_string_process *io_process = t_io_string_process_create(pid, value, IO_TYPE_STDOUT);

    pthread_mutex_lock(&io_mutex);
    t_client_info *io = get_available_io_type(list_io_stdout);

    if (io != NULL) {
        pthread_mutex_lock(&io->internal_mutex);
        io->is_available = false;
        pthread_mutex_unlock(&io->internal_mutex);
        pthread_mutex_unlock(&io_mutex);

        io_string_process_send(io_process, STDOUT, io->fd, &io->network_mutex);

        log_debug(logger, "Proceso %d enviado a IO STDOUT (fd: %d)", pid, io->fd);
    } else {
        list_add(pending_request_io_stdout, io_process);
        pthread_mutex_unlock(&io_mutex);

        log_debug(logger, "No hay dispositivos IO de tipo STDOUT disponibles para procesar la solicitud de stdout del proceso %d. El proceso quedará bloqueado hasta que un dispositivo IO de tipo STDOUT esté disponible.", pid);
    }

    sem_post(&short_term_scheduler_sem);
}

void receive_instruction_sleep (uint32_t *pid, uint32_t *sleep_time, int cpu_fd) {
    int size;
    int offset = 0;
	void *buffer = buffer_receive(&size, cpu_fd);
	if (buffer == NULL) return;

	*pid = uint32_deserialize(buffer, &offset);
	*sleep_time = uint32_deserialize(buffer, &offset);

	free(buffer);
}

void receive_instruction_std (uint32_t *pid, uint32_t *physical_address, uint32_t *to_read, int cpu_fd) {
    int size;
    int offset = 0;
	void *buffer = buffer_receive(&size, cpu_fd);
	if (buffer == NULL) return;

	*pid = uint32_deserialize(buffer, &offset);
	*physical_address = uint32_deserialize(buffer, &offset);
	*to_read = uint32_deserialize(buffer, &offset);

	free(buffer);
}

void io_finish_process(uint32_t pid, t_client_info *io) {
    pthread_mutex_lock(&scheduler_mutex);
    t_process *process = get_process_from_pid(pid);

    if (process != NULL) {
        process_set_ready(process);
        if (process->state == READY) {
            add_process_to_ready_queue(process);
        }
        pthread_mutex_unlock(&scheduler_mutex);

        pthread_mutex_lock(&io->internal_mutex);
        io->is_available = true;
        pthread_mutex_unlock(&io->internal_mutex);

        sem_post(&short_term_scheduler_sem);
    } else {
        pthread_mutex_unlock(&scheduler_mutex);
        log_warning(logger, "Proceso %d no encontrado", pid);
    }
}

void handle_next_operation (t_client_info *io, t_io_type io_type, t_list *pending_io_list, op_code op_code) {
    log_debug(logger, "Hay %d procesos de IO %d pendientes", list_size(pending_io_list), io_type);
	switch (io_type) {
		case IO_TYPE_SLEEP:  {
			pthread_mutex_lock(&io_mutex);

			t_io_numeric_process *pending_process = get_next_io_numeric_process_from_list(pending_io_list);

			if (pending_process == NULL) {
				pthread_mutex_unlock(&io_mutex);
				break;
			}

			pthread_mutex_lock(&io->internal_mutex);
			io->is_available = false;
			pthread_mutex_unlock(&io->internal_mutex);

			pthread_mutex_unlock(&io_mutex);
			
			io_numeric_process_send(pending_process, op_code, io->fd, &io->network_mutex);

			break;
		}

		case IO_TYPE_STDOUT: {
			pthread_mutex_lock(&io_mutex);

			t_io_string_process *pending_process = get_next_io_string_process_from_list(pending_io_list);

			if (pending_process == NULL) {
				pthread_mutex_unlock(&io_mutex);
				break;
			}

			pthread_mutex_lock(&io->internal_mutex);
			io->is_available = false;
			pthread_mutex_unlock(&io->internal_mutex);

			pthread_mutex_unlock(&io_mutex);
			
			io_string_process_send(pending_process, op_code, io->fd, &io->network_mutex);

			break;
		}

		case IO_TYPE_STDIN: {
			pthread_mutex_lock(&io_mutex);
			
            t_io_numeric_process *pending_process = get_next_io_numeric_process_from_list(pending_io_list);

			if (pending_process == NULL) {
				pthread_mutex_unlock(&io_mutex);
				break;
			}

			pthread_mutex_lock(&io->internal_mutex);
			io->is_available = false;
			pthread_mutex_unlock(&io->internal_mutex);

			pthread_mutex_unlock(&io_mutex);
			

            io_numeric_process_send(pending_process, op_code, io->fd, &io->network_mutex);

			break;
		}

		default: {
			log_error(logger, "Tipo de IO desconocido: %d", io_type);
			return;
		}
	}
}
