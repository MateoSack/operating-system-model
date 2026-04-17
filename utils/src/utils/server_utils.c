#include <utils/server_utils.h>

void add_client_to_list (t_list *list, int client_fd, uint32_t id) {
	t_client_info *client = malloc(sizeof(t_client_info));
	client->fd = client_fd;
	client->id = id;
    list_add(list, client);
}

void remove_client_from_list (t_list *list, t_client_info *client) {
    list_remove_element(list, client);
}

uint32_t id_assigner (uint32_t *next_client_id, int client_fd) {
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

int server_start (char *port, t_log *logger) {
	int server_socket;
	int err = 0;

	struct addrinfo hints, *server_info; //, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(NULL, port, &hints, &server_info);

	if (err != 0) {
		log_error(logger, "Getaddrinfo error");
		return -1;
	}

	server_socket = socket(server_info->ai_family,
							server_info->ai_socktype,
							server_info->ai_protocol);

	err = setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

	if (err != 0) {
		log_error(logger, "Setsockopt error");
		return -1;
	}

	err = bind(server_socket, server_info->ai_addr, server_info->ai_addrlen);

	if (err != 0) {
		log_error(logger, "Bind error");
		return -1;
	}

	err = listen(server_socket, SOMAXCONN);

	if (err != 0) {
		log_error(logger, "Listen error");
		return -1;
	}

	freeaddrinfo(server_info);
	log_trace(logger, "Ready to listen to client");

	return server_socket;
}

int server_client_wait (int socket_server) {
	int client_socket = accept(socket_server, NULL, NULL);;

	log_info(logger, "Client connected");

	return client_socket;
}

char* get_port_from_fd (int fd, t_log *logger) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if (getsockname(fd, (struct sockaddr*)&addr, &len) == -1) {
        log_error(logger, "Couldnt get port");
        return NULL;
    }

    int port = ntohs(addr.sin_port);

    char *port_str = malloc(6); // max "65535" + '\0'

    snprintf(port_str, 6, "%d", port);

    return port_str;
}

char* get_ip_from_fd (int fd, t_log *logger) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if (getsockname(fd, (struct sockaddr*)&addr, &len) == -1) {
        log_error(logger, "Couldnt get ip");
        return NULL;
    }

    char *ip = malloc(INET_ADDRSTRLEN);

    inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);

	return ip;
}
