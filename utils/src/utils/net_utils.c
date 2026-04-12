#include <utils/net_utils.h>


t_log *logger;

int server_start (t_log *logger) {
	int server_socket;
	int err = 0;

	struct addrinfo hints, *server_info; //, *p;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(NULL, PORT, &hints, &server_info);

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

int connection_create (char *ip, char *port, t_log *logger) {
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int err;

	err = getaddrinfo(ip, port, &hints, &server_info);

	if (err != 0) {
		log_error(logger, "getaddrinfo error");
		return -1;
	}

	int client_socket = socket(server_info->ai_family,
								server_info->ai_socktype,
								server_info->ai_protocol);

	err = connect(client_socket, server_info->ai_addr, server_info->ai_addrlen);

	if (err != 0) {
		log_error(logger, "connect error");
		return -1;
	}

	freeaddrinfo(server_info);

	return client_socket;
}

void connection_liberate (int client_socket) {
	close(client_socket);
}

int operation_receive (int client_socket) {
	int cod_op;
	if (recv(client_socket, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(client_socket);
		return -1;
	}
}

void *buffer_receive (int *size, int client_socket) {
	void *buffer;

	recv(client_socket, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(client_socket, buffer, *size, MSG_WAITALL);

	return buffer;
}

void message_receive (int client_socket) {
	int size;
	char *buffer = buffer_receive(&size, client_socket);
	log_info(logger, "Received message: %s", buffer);
	free(buffer);
}

void message_send (char *message, int client_socket) {
	t_package *package = malloc(sizeof(t_package));

	package->op_code = MESSAGE;
	package->buffer = malloc(sizeof(t_buffer));
	package->buffer->size = strlen(message) + 1;
	package->buffer->stream = malloc(package->buffer->size);
	memcpy(package->buffer->stream, message, package->buffer->size);

	int bytes = package->buffer->size + 2 * sizeof(int);

	void *to_send = package_serialize(package, bytes);

	send(client_socket, to_send, bytes, 0);

	free(to_send);
	remove_package(package);
}

void buffer_create (t_package *package)
{
	package->buffer = malloc(sizeof(t_buffer));
	package->buffer->size = 0;
	package->buffer->stream = NULL;
}

t_package *package_create (void)
{
	t_package *package = malloc(sizeof(t_package));
	package->op_code = PACKAGE;
	buffer_create(package);
	return package;
}

void package_add (t_package *package, void *value, int size)
{
	package->buffer->stream = realloc(package->buffer->stream, package->buffer->size + size + sizeof(int));

	memcpy(package->buffer->stream + package->buffer->size, &size, sizeof(int));
	memcpy(package->buffer->stream + package->buffer->size + sizeof(int), value, size);

	package->buffer->size += size + sizeof(int);
}

void package_send (t_package *package, int client_socket)
{
	int bytes = package->buffer->size + 2 * sizeof(int);
	void *to_send = package_serialize(package, bytes);

	send(client_socket, to_send, bytes, 0);

	free(to_send);
}

void package_delete (t_package *package)
{
	free(package->buffer->stream);
	free(package->buffer);
	free(package);
}

void pcb_handle (t_pcb *pcb, int client_socket) {
    t_package *package = package_create();
    package_add(package, &pcb->pid, sizeof(uint32_t));
    package_add(package, &pcb->state, sizeof(int));
    package_add(package, &pcb->context.pc, sizeof(uint32_t));
    package_add(package, &pcb->context.ax, sizeof(uint8_t));
	package_add(package, &pcb->context.bx, sizeof(uint8_t));
	package_add(package, &pcb->context.cx, sizeof(uint8_t));
	package_add(package, &pcb->context.dx, sizeof(uint8_t));
    package_add(package, &pcb->context.eax, sizeof(uint32_t));
	package_add(package, &pcb->context.ebx, sizeof(uint32_t));
	package_add(package, &pcb->context.ecx, sizeof(uint32_t));
	package_add(package, &pcb->context.edx, sizeof(uint32_t));
    package_add(package, &pcb->context.si, sizeof(uint32_t));
    package_add(package, &pcb->context.di, sizeof(uint32_t));
    package_send(package, client_socket);
    package_delete(package);
}

uint32_t int32_deserialize(void *buffer, int *offset) {
	int size;
	uint32_t value;
	memcpy(&size, buffer + *offset, sizeof(int));
	*offset += sizeof(int);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

uint8_t int8_deserialize(void *buffer, int *offset) {
	int size;
	uint32_t value;
	memcpy(&size, buffer + *offset, sizeof(int));
	*offset += sizeof(int);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

t_pcb *pcb_receive(int socket_cliente) {
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, socket_cliente);
    t_pcb *pcb = malloc(sizeof(t_pcb));

    pcb->pid = int32_deserialize(buffer, &offset);
    pcb->state = (t_process_state)int32_deserialize(buffer, &offset);
	pcb->context.pc = int32_deserialize(buffer, &offset);
	pcb->context.ax = int8_deserialize(buffer, &offset);
	pcb->context.bx = int8_deserialize(buffer, &offset);
	pcb->context.cx = int8_deserialize(buffer, &offset);
	pcb->context.dx = int8_deserialize(buffer, &offset);
    pcb->context.eax = int32_deserialize(buffer, &offset);
    pcb->context.ebx = int32_deserialize(buffer, &offset);
    pcb->context.ecx = int32_deserialize(buffer, &offset);
	pcb->context.edx = int32_deserialize(buffer, &offset);
    pcb->context.si = int32_deserialize(buffer, &offset);
    pcb->context.di = int32_deserialize(buffer, &offset);

    free(buffer);
    return pcb;
}

void *package_serialize(t_package *package, int bytes)
{
	void *buffer = malloc(bytes);
	int offset = 0;

	memcpy(buffer + offset, &(package->op_code), sizeof(int));
	offset += sizeof(int);
	memcpy(buffer + offset, &(package->buffer->size), sizeof(int));
	offset += sizeof(int);
	memcpy(buffer + offset, package->buffer->stream, package->buffer->size);
	offset += package->buffer->size;

	return buffer;
}