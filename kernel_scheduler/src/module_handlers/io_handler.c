#include "io_handler.h"

void io_handler (int io_fd) {
	t_io_type io_type = t_io_type_receive(io_fd);
	uint32_t id = id_assigner(&next_io_id, io_fd, &io_id_mutex);

	t_client_info *io = NULL;

	t_list *io_list = NULL;
	t_list *pending_io_list = NULL;

	op_code standard_op;

	if (io_type == IO_TYPE_STDIN) {
		io = add_client_to_list(list_io_stdin, io_fd, id);
		log_info(logger, "IO STDIN %d conectada (total: %d)", id, list_size(list_io_stdin));
		io_list = list_io_stdin;
		pending_io_list = pending_request_io_stdin;
		standard_op = STDIN;
	} else if (io_type == IO_TYPE_STDOUT) {
		io = add_client_to_list(list_io_stdout, io_fd, id);
		log_info(logger, "IO STDOUT %d conectada (total: %d)", id, list_size(list_io_stdout));
		io_list = list_io_stdout;
		pending_io_list = pending_request_io_stdout;
		standard_op = STDOUT;
	} else if (io_type == IO_TYPE_SLEEP) {
		io = add_client_to_list(list_io_sleep, io_fd, id);
		log_info(logger, "IO SLEEP %d conectada (total: %d)", id, list_size(list_io_sleep));
		io_list = list_io_sleep;
		pending_io_list = pending_request_io_sleep;
		standard_op = SLEEP;
	} else {
		log_error(logger, "Tipo de IO desconocido recibido: %d", io_type);
		close(io_fd);
		free(io);
		return;
	}

	handle_next_operation(io, io_type, pending_io_list, standard_op);

	while (1) {
		//Handle connection with IO
		int op = operation_receive(io_fd);

		switch (op) {
			case -1: {
				log_warning(logger, "IO %d desconectada", id);
				close(io_fd);
				remove_client_from_list(io_list, io);
				free(io);
				return;
			}

			case CONFIRMATION: {
				uint32_t pid = uint32_decode(io_fd);
				log_info(logger, "## PID: %d finalizó IO y pasa a READY / SUSP. READY", pid);

				io_finish_process(pid, io);

				handle_next_operation(io, io_type, pending_io_list, standard_op);

				break;
			}

			case STDIN: {
				t_io_string_process *io_process = io_string_process_receive(io_fd);

				if (io_process == NULL) break;

				// Should send to kernel memory

				log_debug(logger, "Proceso %d realizó una operación de IO STDIN con valor: %s", io_process->pid, io_process->value);
					
				log_info(logger, "## PID: %d finalizó IO y pasa a READY / SUSP. READY", io_process->pid);

				io_finish_process(io_process->pid, io);

				free(io_process);

				handle_next_operation(io, io_type, pending_io_list, standard_op);

				break;
			}

			default: {
				log_warning(logger, "Operacion desconocida recibida de IO %d: %d", id, op);
				break;
			}
		}
	}
}

void io_finish_process(uint32_t pid, t_client_info *io) {
    pthread_mutex_lock(&scheduler_mutex);
    t_process *process = get_process_from_pid(pid);

    if (process != NULL) {
        process_set_state(process, READY, logger);
        add_process_to_list(ready_queue, process);
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
	switch (io_type) {
		case IO_TYPE_SLEEP:
			pthread_mutex_lock(&io_mutex); {

			t_io_numeric_process *pending_process = get_next_io_numeric_process_from_list(pending_io_list);

			if (pending_process == NULL) {
				pthread_mutex_unlock(&io_mutex);
				break;
			}

			pthread_mutex_lock(&io->internal_mutex);
			io->is_available = false;
			pthread_mutex_unlock(&io->internal_mutex);

			pthread_mutex_unlock(&io_mutex);
			
			pthread_mutex_lock(&io->network_mutex);
			io_numeric_process_send(pending_process, op_code, io->fd);
			pthread_mutex_unlock(&io->network_mutex);
			break;
		}

		case IO_TYPE_STDOUT: {
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
			
			pthread_mutex_lock(&io->network_mutex);
			io_numeric_process_send(pending_process, op_code, io->fd);
			pthread_mutex_unlock(&io->network_mutex);
			break;
		}

		case IO_TYPE_STDIN: {
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
			
			pthread_mutex_lock(&io->network_mutex);
			io_string_process_send(pending_process, op_code, io->fd);
			pthread_mutex_unlock(&io->network_mutex);
			break;
		}

		default: {
			log_error(logger, "Tipo de IO desconocido: %d", io_type);
			return;
		}
	}
}
