#include "io_handler.h"

void io_handler (int io_fd) {
	uint32_t id = id_assigner(&next_io_id, io_fd, &io_id_mutex);

	t_client_info *io = malloc(sizeof(t_client_info));
	io = add_client_to_list(list_io, io_fd, id);
	log_info(logger, "IO %d connected (total: %d)", id, list_size(list_io));

	while (1) {
		//Handle connection with IO
		int op = operation_receive(io_fd);
        if (op == -1) {
            log_warning(logger, "IO %d disconnected", id);
			close(io_fd);
			remove_client_from_list(list_io, io);
			free(io);
            break;
        }
	}
}
