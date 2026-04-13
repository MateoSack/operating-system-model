#include <utils/server_utils.h>

void add_client_to_list (t_list *list, int client_fd, int id) {
	t_client_info *client = malloc(sizeof(t_client_info));
	client->fd = client_fd;
	client->id = id;
    list_add(list, client);
}

int id_assigner (int *next_client_id, int client_fd) {
	t_package *pkg = package_create();
    package_add(pkg, next_client_id, sizeof(int));
	(*next_client_id)++;
	package_send(pkg, client_fd);
    package_delete(pkg);
    return (*next_client_id) - 1;
}

t_module_id handshake_receiver (int client_fd) {
	int cod_op = operation_receive(client_fd);
    if (cod_op != HANDSHAKE) {
        log_error(logger, "Expected HANDSHAKE, received: %d", cod_op);
        close(client_fd);
        return -1;
    }

    t_module_id module_id = t_module_id_receive(client_fd);
	return module_id;
}