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

				pthread_mutex_lock(&pending_io_stdin_reading_mutex);
				t_pending_stdin *pending_stdin = get_pending_stdin_from_pid(io_process->pid);
				if (pending_stdin == NULL) {
					log_error(logger, "No se pudo conseguir la operacion stdin pendiente");
					pthread_mutex_unlock(&pending_io_stdin_reading_mutex);
					break;
				}
				uint32_t physical_address = pending_stdin->physical_address;
				list_remove_element(pending_io_stdin_reading, pending_stdin);
				pthread_mutex_unlock(&pending_io_stdin_reading_mutex);

				free(pending_stdin);

				send_memory_write(io_process->pid, physical_address, io_process->value);

				pthread_mutex_lock(&io->internal_mutex);
				io->is_available = true;
				pthread_mutex_unlock(&io->internal_mutex);

				log_debug(logger, "Proceso %d realizó una operación de IO STDIN con valor: %s", io_process->pid, io_process->value);

				t_process *process = get_process_from_pid(io_process->pid);

        		pthread_t thread;
				pthread_create(&thread, NULL, wait_memory_write_confirmation, process);
				pthread_detach(thread);

				free(io_process->value);
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
