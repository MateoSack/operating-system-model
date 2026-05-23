#include <utils/net_utils.h>

int connection_create (char *ip, char *port, t_log *logger) { // Returns client socket fd, or -1 on error
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
		log_error(logger, "error de connección");
		return -1;
	}

	freeaddrinfo(server_info);

	return client_socket;
}

void connection_liberate (int client_socket) { // Closes the connection with the client
	close(client_socket);
}

int operation_receive (int client_socket) { // Receives the operation code of the incoming package, returns -1 on error or disconnection
	int cod_op;
	if (recv(client_socket, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(client_socket);
		return -1;
	}
}

void *buffer_receive (int *size, int client_socket) { // Receives a buffer from the client, returns NULL on error or disconnection
	void *buffer;

	recv(client_socket, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(client_socket, buffer, *size, MSG_WAITALL);

	return buffer;
}

char *message_receive (t_log *logger, int client_socket) { // Receives a message (char*) from the client, returns NULL on error or disconnection
	int op_code = operation_receive(client_socket);

	if (op_code != MESSAGE) {
		log_error(logger, "Error recuperando mensaje");
        return NULL;
    }

	int size;
	char *buffer = buffer_receive(&size, client_socket);
	log_debug(logger, "Mensaje recibido: %s", buffer);
	return buffer;
}

char *message_decode (int client_socket) { // Receives a message (char*) from the client, use only if already did an operation_receive
	int size;
	char *buffer = buffer_receive(&size, client_socket);
	return buffer;
}

void message_send (char *message, int client_socket) { // Sends a message (char*) to the client
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
	package_delete(package);
}

void message_send_with_op_code (char *message, op_code op_code, int client_socket) { // Sends a message (char*) to the client with a specific operation code (use for operations that expect a message but not necessarily a PACKAGE, like MUTEX_CREATE, MUTEX_LOCK and MUTEX_UNLOCK)
	t_package *package = malloc(sizeof(t_package));

	package->op_code = op_code;
	package->buffer = malloc(sizeof(t_buffer));
	package->buffer->size = strlen(message) + 1;
	package->buffer->stream = malloc(package->buffer->size);
	memcpy(package->buffer->stream, message, package->buffer->size);

	int bytes = package->buffer->size + 2 * sizeof(int);

	void *to_send = package_serialize(package, bytes);

	send(client_socket, to_send, bytes, 0);

	free(to_send);
	package_delete(package);
}

void buffer_create (t_package *package) { // Initializes the buffer of a package
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

void package_add (t_package *package, void *value, int size) { // Adds a value to the buffer of a package
	package->buffer->stream = realloc(package->buffer->stream, package->buffer->size + size + sizeof(int));

	memcpy(package->buffer->stream + package->buffer->size, &size, sizeof(int));
	memcpy(package->buffer->stream + package->buffer->size + sizeof(int), value, size);

	package->buffer->size += size + sizeof(int);
}

void package_send (t_package *package, int client_socket) { // Sends a package to the client
	int bytes = package->buffer->size + 2 * sizeof(int);
	void *to_send = package_serialize(package, bytes);

	send(client_socket, to_send, bytes, 0);

	free(to_send);
}

void package_delete (t_package *package) { // Deletes a package and its buffer
	free(package->buffer->stream);
	free(package->buffer);
	free(package);
}

uint32_t uint32_deserialize(void *buffer, int *offset) { // Deserializes a uint32_t from a buffer, updating the offset
	int size;
	uint32_t value;
	memcpy(&size, buffer + *offset, sizeof(uint32_t));
	*offset += sizeof(uint32_t);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

uint8_t uint8_deserialize(void *buffer, int *offset) { // Deserializes a uint8_t from a buffer, updating the offset
	int size;
	uint8_t value;
	memcpy(&size, buffer + *offset, sizeof(uint8_t));
	*offset += sizeof(uint8_t);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

t_module_id t_module_id_deserialize(void *buffer, int *offset) { // Deserializes a t_module_id from a buffer, updating the offset
	int size;
	t_module_id value;
	memcpy(&size, buffer + *offset, sizeof(t_module_id));
	*offset += sizeof(t_module_id);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

void *package_serialize(t_package *package, int bytes) { // Serializes a package into a buffer, returns the buffer
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

void t_module_id_send (int server_fd, t_module_id module_id, t_log *logger) { // Sends a t_module_id to the server as part of the handshake process
    t_package *pkg = package_create();
    pkg->op_code = HANDSHAKE;  // Set to HANDSHAKE instead of PACKAGE
    package_add(pkg, &module_id, sizeof(t_module_id));
    package_send(pkg, server_fd);
    package_delete(pkg);
    log_debug(logger, "t_module_id enviado a: %d", server_fd);
}

t_module_id t_module_id_decode (int client_fd) { // Receives a t_module_id from the client, returns the module_id. Use only after receiving a HANDSHAKE operation code
	int size;
	int offset = 0;
    void *buffer = buffer_receive(&size, client_fd);
    t_module_id module_id = t_module_id_deserialize(buffer, &offset);
    free(buffer);
	return module_id;
}

uint32_t uint32_receive (int client_fd) { // Receives a uint32_t from the client, returns the value
    int op_code = operation_receive(client_fd);
    if (op_code != PACKAGE && op_code != PROCESS_CREATE) {
        log_error(logger, "uint32_receive: se esperaba PACKAGE, se recibió %d", op_code);
        return 0;
    }

	int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, client_fd);
    uint32_t value = uint32_deserialize(buffer, &offset);
    free(buffer);
	return value;
}

void uint32_send (int client_fd, uint32_t value) { // Sends a uint32_t to the client as part of a package
	t_package *pkg = package_create();
    package_add(pkg, &value, sizeof(uint32_t));
	package_send(pkg, client_fd);
    package_delete(pkg);
}

void send_credentials_list (int fd, t_list *list, t_log *logger) { // Sends a list of t_module_credentials to the client as part of a package
	t_package *pkg = package_create();
	for(int i = 0; i < list_size(list); i++) {
		t_module_credentials *credentials = list_get(list, i);
		package_add(pkg, credentials->ip, strlen(credentials->ip) + 1);
		package_add(pkg, credentials->port, strlen(credentials->port) + 1);
		package_add(pkg, &credentials->id, sizeof(credentials->id));
	}

	package_send(pkg, fd);
	package_delete(pkg);
	log_debug(logger, "Paquete de credenciales enviado a fd: %d", fd);
}

t_list *receive_credentials_list (int socket_cliente) { // Receives a list of t_module_credentials from the client as part of a package, returns the list
    int op_code = operation_receive(socket_cliente);
    if (op_code != PACKAGE) {
        return NULL;
    }

	t_list *list = list_create();

    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, socket_cliente);

	while (offset < size) {
		t_module_credentials *cred = malloc(sizeof(t_module_credentials));

		// Deserialize ip: size + string
		int field_size;
		memcpy(&field_size, buffer + offset, sizeof(int));
		offset += sizeof(int);
		cred->ip = malloc(field_size);
		memcpy(cred->ip, buffer + offset, field_size);
		offset += field_size;

		// Deserialize port: size + string
		memcpy(&field_size, buffer + offset, sizeof(int));
		offset += sizeof(int);
		cred->port = malloc(field_size);
		memcpy(cred->port, buffer + offset, field_size);
		offset += field_size;

		// Deserialize id: size + uint32_t
		memcpy(&field_size, buffer + offset, sizeof(int));
		offset += sizeof(int);
		memcpy(&cred->id, buffer + offset, field_size);
		offset += field_size;

		list_add(list, cred);
	}

    free(buffer);
    return list;
}

void send_credentials (int fd, t_module_credentials *cred, t_log *logger) { // Sends a t_module_credentials to the client as part of a package
	t_package *pkg = package_create();
	pkg->op_code = CREDENTIALS_UPDATE;
	package_add(pkg, cred->ip, strlen(cred->ip) + 1);
	package_add(pkg, cred->port, strlen(cred->port) + 1);
	package_add(pkg, &cred->id, sizeof(cred->id));
	package_send(pkg, fd);
	package_delete(pkg);
	log_debug(logger, "Credenciales enviadas a fd: %d", fd);
}

t_module_credentials *receive_credentials (int socket_cliente) { // Receives a t_module_credentials from the client as part of a package, returns the credentials
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, socket_cliente);

	t_module_credentials *cred = malloc(sizeof(t_module_credentials));

	// Deserialize ip: size + string
	int field_size;
	memcpy(&field_size, buffer + offset, sizeof(int));
	offset += sizeof(int);
	cred->ip = malloc(field_size);
	memcpy(cred->ip, buffer + offset, field_size);
	offset += field_size;

	// Deserialize port: size + string
	memcpy(&field_size, buffer + offset, sizeof(int));
	offset += sizeof(int);
	cred->port = malloc(field_size);
	memcpy(cred->port, buffer + offset, field_size);
	offset += field_size;

	// Deserialize id: size + uint32_t
	memcpy(&field_size, buffer + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&cred->id, buffer + offset, field_size);
	offset += field_size;

    free(buffer);
    return cred;
}

void t_module_credentials_destroyer (void *ptr) { // Destroys a t_module_credentials, use as list_destroy_and_destroy_elements destroyer
	t_module_credentials *credentials = (t_module_credentials *) ptr;
	free(credentials->ip);
	free(credentials->port);
	free(credentials);
}

t_client_info *add_client_to_list (t_list *list, int client_fd, uint32_t id) { // Adds a client to the list of clients, returns the client info
	t_client_info *client = malloc(sizeof(t_client_info));
	client->fd = client_fd;
	client->id = id;
    list_add(list, client);
	log_debug(logger, "Cliente agregado a la lista con fd: %d, id: %d", client_fd, id);
	return client;
}

void remove_client_from_list (t_list *list, t_client_info *client) { // Removes a client from the list of clients
    list_remove_element(list, client);
}

uint32_t uint32_decode (int client_fd) { //Returns uint32 from client, use only if already did an operation_receive
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, client_fd);
    uint32_t value = uint32_deserialize(buffer, &offset);
    free(buffer);
    return value;
}

void send_confirmation (int client_fd) { // Sends a confirmation package to the client
	t_package *pkg = package_create();
	pkg->op_code = CONFIRMATION;
	package_send(pkg, client_fd);
    package_delete(pkg);
}

void wait_confirmation (int client_fd) { // Waits for a confirmation package from the client, shouldnt be used because of busy waiting
	int op_code = operation_receive(client_fd);
	if (op_code != CONFIRMATION) {
		log_error(logger, "Se esperaba CONFIRMATION, se recibió: %d", op_code);
	}
}

void t_io_type_send (int server_fd, t_io_type module_type, t_log *logger) { // Sends a t_io_type to the server as part of the handshake process
    t_package *pkg = package_create();
    pkg->op_code = HANDSHAKE;  // Set to HANDSHAKE instead of PACKAGE
    package_add(pkg, &module_type, sizeof(t_io_type));
    package_send(pkg, server_fd);
    package_delete(pkg);
    log_debug(logger, "t_io_type enviado a: %d", server_fd);
}

t_io_type t_io_type_receive (int client_fd) { // Receives a t_io_type from the client, returns the io_type
    int op_code = operation_receive(client_fd);
    if (op_code != HANDSHAKE) {
        log_error(logger, "t_io_type_receive: se esperaba HANDSHAKE, se recibió %d", op_code);
        return 0;
    }

	int size;
	int offset = 0;
    void *buffer = buffer_receive(&size, client_fd);
    t_io_type io_type = t_io_type_deserialize(buffer, &offset);
    free(buffer);
	return io_type;
}

t_io_type t_io_type_deserialize(void *buffer, int *offset) { // Deserializes a t_io_type from a buffer, updating the offset
	int size;
	t_io_type value;
	memcpy(&size, buffer + *offset, sizeof(t_io_type));
	*offset += sizeof(t_io_type);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}
