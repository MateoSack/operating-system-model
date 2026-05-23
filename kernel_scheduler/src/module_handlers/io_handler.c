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
        if (op == -1) {
            log_warning(logger, "IO %d desconectada", id);
			close(io_fd);
			remove_client_from_list(io_list, io);
			free(io);
            break;
        }
	}
}
