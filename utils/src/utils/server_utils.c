#include <utils/server_utils.h>

t_module_id handshake_receiver (int client_fd) { // Receives the handshake from the client, returns the module_id of the client or -1 on error
	int cod_op = operation_receive(client_fd);
    if (cod_op != HANDSHAKE) {
        log_error(logger, "Expected HANDSHAKE, received: %d", cod_op);
        close(client_fd);
        return -1;
    }

    t_module_id module_id = t_module_id_decode(client_fd);
	return module_id;
}

int server_start (char *port, t_log *logger) { // Starts the server, returns the server socket fd or -1 on error
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

int server_client_wait (int socket_server) { // Waits for a client to connect, returns the client socket fd or -1 on error
	int client_socket = accept(socket_server, NULL, NULL);;

	log_info(logger, "Client connected");

	return client_socket;
}

char* get_port_from_fd (int fd, t_log *logger) { // Gets the port of a socket fd, returns it as a string or NULL on error
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

char* get_ip_from_fd (int fd, t_log *logger) { // Gets the ip of a socket fd, returns it as a string or NULL on error
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

uint32_t id_assigner (uint32_t *current_max_id, int fd, pthread_mutex_t *mutex) { // Assign a new ID for a CPU or IO, based on the current maximum ID and the list of connected clients, sends it to the client and returns the assigned ID
    pthread_mutex_lock(mutex);
    uint32_t id = *current_max_id;
    (*current_max_id)++;
    pthread_mutex_unlock(mutex);

    uint32_send(fd, id);

    return id;
}
