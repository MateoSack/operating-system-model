#include "io_handler.h"

void io_handler (int io_fd) {
	t_io_type io_type = t_io_type_receive(io_fd);
	uint32_t id = id_assigner(&next_io_id, io_fd, &io_id_mutex);

	t_client_info *io = malloc(sizeof(t_client_info));

	t_list *io_list = NULL;

	if (io_type == IO_TYPE_STDIN) {
		io = add_client_to_list(list_io_stdin, io_fd, id);
		log_info(logger, "IO STDIN %d conectada (total: %d)", id, list_size(list_io_stdin));
		io_list = list_io_stdin;
	} else if (io_type == IO_TYPE_STDOUT) {
		io = add_client_to_list(list_io_stdout, io_fd, id);
		log_info(logger, "IO STDOUT %d conectada (total: %d)", id, list_size(list_io_stdout));
		io_list = list_io_stdout;
	} else if (io_type == IO_TYPE_SLEEP) {
		io = add_client_to_list(list_io_sleep, io_fd, id);
		log_info(logger, "IO SLEEP %d conectada (total: %d)", id, list_size(list_io_sleep));
		io_list = list_io_sleep;
	} else {
		log_error(logger, "Tipo de IO desconocido recibido: %d", io_type);
		close(io_fd);
		free(io);
		return;
	}

	while (1) {
		//Handle connection with IO
		int op = operation_receive(io_fd);

		switch (op) {
			case -1: {
				log_warning(logger, "IO %d desconectada", id);
				close(io_fd);
				remove_client_from_list(io_list, io);
				free(io);
				break;
			}

			case CONFIRMATION: {
				uint32_t pid = uint32_decode(io_fd);
				log_info(logger, "## PID: %d - IO %d - Confirmación de finalización recibida", pid, id);

				pthread_mutex_lock(&scheduler_mutex);
				t_process *process = get_process_from_pid(pid);
				if (process != NULL) {
					process_set_state(process, READY, logger);
					add_process_to_list(ready_queue, process);
					io->is_available = true;
					pthread_mutex_unlock(&scheduler_mutex);

					sem_post(&short_term_scheduler_sem);
				} else {
					pthread_mutex_unlock(&scheduler_mutex);
					log_warning(logger, "Proceso %d no encontrado para confirmar finalización de IO", pid);
				}

				break;
			}

			case STDIN: {
				t_io_string_process *io_process = io_string_process_receive(io_fd);

				if (io_process == NULL) break;

				// Should send to kernel memory

				log_debug(logger, "Proceso %d realizó una operación de IO STDIN con valor: %s", io_process->pid, io_process->value);
					
				log_info(logger, "## PID: %d finalizó IO y pasa a READY / SUSP. READY", io_process->pid);

				pthread_mutex_lock(&scheduler_mutex);
				t_process *process = get_process_from_pid(io_process->pid);
				if (process != NULL) {
					process_set_state(process, READY, logger);
					add_process_to_list(ready_queue, process);
					io->is_available = true;
					pthread_mutex_unlock(&scheduler_mutex);

					sem_post(&short_term_scheduler_sem);
				} else {
					pthread_mutex_unlock(&scheduler_mutex);
					log_warning(logger, "Proceso %d no encontrado para confirmar finalización de IO", io_process->pid);
				}

				free(io_process);

				t_io_string_process *pending_process = get_next_io_string_process_from_list(pending_request_io_stdin);

				if (pending_process == NULL) break;

				break;
			}

			default: {
				log_warning(logger, "Operacion desconocida recibida de IO %d: %d", id, op);
				break;
			}
		}
	}
}
