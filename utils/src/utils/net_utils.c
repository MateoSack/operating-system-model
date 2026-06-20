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
	
	if (*size == 0) {
		return NULL;
	}
	
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

void message_send (char *message, int client_socket, pthread_mutex_t *mutex) { // Sends a message (char*) to the client
	t_package *package = malloc(sizeof(t_package));

	package->op_code = MESSAGE;
	package->buffer = malloc(sizeof(t_buffer));
	package->buffer->size = strlen(message) + 1;
	package->buffer->stream = malloc(package->buffer->size);
	memcpy(package->buffer->stream, message, package->buffer->size);

	int bytes = package->buffer->size + 2 * sizeof(int);

	void *to_send = package_serialize(package, bytes);

	pthread_mutex_lock(mutex);
	send(client_socket, to_send, bytes, 0);
	pthread_mutex_unlock(mutex);

	free(to_send);
	package_delete(package);
}

void message_send_with_op_code (char *message, op_code op_code, int client_socket, pthread_mutex_t *mutex) { // Sends a message (char*) to the client with a specific operation code (use for operations that expect a message but not necessarily a PACKAGE, like MUTEX_CREATE, MUTEX_LOCK and MUTEX_UNLOCK)
	t_package *package = malloc(sizeof(t_package));

	package->op_code = op_code;
	package->buffer = malloc(sizeof(t_buffer));
	package->buffer->size = strlen(message) + 1;
	package->buffer->stream = malloc(package->buffer->size);
	memcpy(package->buffer->stream, message, package->buffer->size);

	int bytes = package->buffer->size + 2 * sizeof(int);

	void *to_send = package_serialize(package, bytes);

	pthread_mutex_lock(mutex);
	send(client_socket, to_send, bytes, 0);
	pthread_mutex_unlock(mutex);

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

void package_string_add(t_package *package, char *message) {
    int size = strlen(message) + 1;
    package_add(package, message, size);
}

void package_send (t_package *package, int client_socket, pthread_mutex_t *mutex) { // Sends a package to the client, uses a mutex to ensure that the sending operation is thread-safe
	int bytes = package->buffer->size + 2 * sizeof(int);
	void *to_send = package_serialize(package, bytes);

	pthread_mutex_lock(mutex);
	send(client_socket, to_send, bytes, 0);
	pthread_mutex_unlock(mutex);

	free(to_send);
}

void package_delete (t_package *package) { // Deletes a package and its buffer
	free(package->buffer->stream);
	free(package->buffer);
	free(package);
}

char *string_deserialize(void *buffer, int *offset) { // Deserializes a char* from a buffer, updating the offset
    int size;
    memcpy(&size, buffer + *offset, sizeof(int));
    *offset += sizeof(int);
    char *value = malloc(size);
    memcpy(value, buffer + *offset, size);
    *offset += size;
    return value;
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
	memcpy(&size, buffer + *offset, sizeof(int));
	*offset += sizeof(int);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

bool bool_deserialize(void *buffer, int *offset) { // Deserializes a bool from a buffer, updating the offset
	int size;
	bool value;
	memcpy(&size, buffer + *offset, sizeof(int));
	*offset += sizeof(int);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

t_module_id t_module_id_deserialize(void *buffer, int *offset) { // Deserializes a t_module_id from a buffer, updating the offset
	int size;
	t_module_id value;
	memcpy(&size, buffer + *offset, sizeof(int));
	*offset += sizeof(int);
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

void t_module_id_send (int server_fd, t_module_id module_id, t_log *logger, pthread_mutex_t *mutex) { // Sends a t_module_id to the server as part of the handshake process
    t_package *pkg = package_create();
    pkg->op_code = HANDSHAKE;  // Set to HANDSHAKE instead of PACKAGE
    package_add(pkg, &module_id, sizeof(t_module_id));
    package_send(pkg, server_fd, mutex);
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

void uint32_send (int client_fd, uint32_t value, pthread_mutex_t *mutex) { // Sends a uint32_t to the client as part of a package
	t_package *pkg = package_create();
    package_add(pkg, &value, sizeof(uint32_t));
	package_send(pkg, client_fd, mutex);
    package_delete(pkg);
}

void send_credentials_list (int fd, t_list *list, t_log *logger, pthread_mutex_t *mutex) { // Sends a list of t_memory_stick_credentials to the client as part of a package
	t_package *pkg = package_create();
	pkg->op_code = PACKAGE;
	for(int i = 0; i < list_size(list); i++) {
		t_memory_stick_credentials *credentials = list_get(list, i);
		package_add(pkg, credentials->ip, strlen(credentials->ip) + 1);
		package_add(pkg, credentials->port, strlen(credentials->port) + 1);
		package_add(pkg, &credentials->id, sizeof(credentials->id));
		package_add(pkg, &credentials->size, sizeof(credentials->size));
	}

	package_send(pkg, fd, mutex);
	package_delete(pkg);
	log_info(logger, "Paquete de credenciales enviado a fd: %d, elementos: %d", fd, list_size(list));
}

t_list *receive_credentials_list (int socket_cliente) { // Receives a list of t_memory_stick_credentials from the client as part of a package, returns the list
    int op_code = operation_receive(socket_cliente);
    if (op_code != PACKAGE) {
        log_error(logger, "receive_credentials_list: se esperaba PACKAGE, se recibió %d", op_code);
        return NULL;
    }

	t_list *list = list_create();

    int size;
    int offset = 0;
	log_debug(logger, "Leyendo el buffer para receive_credentials_list");
    void *buffer = buffer_receive(&size, socket_cliente);
	log_debug(logger, "Buffer recibido para receive_credentials_list");

	if (size == 0) {
    free(buffer);
    log_info(logger, "Lista de credenciales vacía");
    return list;
	}

	while (offset < size) {
		t_memory_stick_credentials *cred = malloc(sizeof(t_memory_stick_credentials));

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

		// Deserialize size: size + uint32_t
		memcpy(&field_size, buffer + offset, sizeof(int));
		offset += sizeof(int);
		memcpy(&cred->size, buffer + offset, field_size);
		offset += field_size;

		list_add(list, cred);
	}

	log_info(logger, "Lista de credenciales recibida con %d elementos", list_size(list));

    free(buffer);
    return list;
}

void send_credentials (int fd, t_memory_stick_credentials *cred, t_log *logger, pthread_mutex_t *mutex) { // Sends a t_memory_stick_credentials to the client as part of a package
	t_package *pkg = package_create();
	pkg->op_code = CREDENTIALS_UPDATE;
	package_add(pkg, cred->ip, strlen(cred->ip) + 1);
	package_add(pkg, cred->port, strlen(cred->port) + 1);
	package_add(pkg, &cred->id, sizeof(cred->id));
	package_add(pkg, &cred->size, sizeof(cred->size));
	package_send(pkg, fd, mutex);
	package_delete(pkg);
	log_debug(logger, "Credenciales enviadas a fd: %d", fd);
}

t_memory_stick_credentials *receive_credentials (int socket_cliente) { // Receives a t_memory_stick_credentials from the client as part of a package, returns the credentials
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, socket_cliente);

	t_memory_stick_credentials *cred = malloc(sizeof(t_memory_stick_credentials));

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

	// Deserialize size: size + uint32_t
	memcpy(&field_size, buffer + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&cred->size, buffer + offset, field_size);
	offset += field_size;

    free(buffer);
    return cred;
}

void t_memory_stick_credentials_destroyer (void *ptr) { // Destroys a t_memory_stick_credentials, use as list_destroy_and_destroy_elements destroyer
	t_memory_stick_credentials *credentials = (t_memory_stick_credentials *) ptr;
	free(credentials->ip);
	free(credentials->port);
	free(credentials);
}

t_client_info *add_client_to_list (t_list *list, int client_fd, uint32_t id) { // Adds a client to the list of clients, returns the client info
	t_client_info *client = create_client_info(client_fd, id);
    list_add(list, client);
	log_debug(logger, "Cliente agregado a la lista con fd: %d, id: %d", client_fd, id);
	return client;
}

t_client_info *create_client_info (int client_fd, uint32_t id) { // Creates a t_client_info struct, returns the pointer to the struct
	t_client_info *client = malloc(sizeof(t_client_info));
	client->fd = client_fd;
	client->id = id;
	client->is_available = true;
	client->is_evicting = false;
	pthread_mutex_init(&client->internal_mutex, NULL);
	pthread_mutex_init(&client->network_mutex, NULL);
	sem_init(&client->response_sem, 0, 0);
	return client;
}

void destroy_client(void *ptr) { // Destroys a client_info struct, closing the connection and freeing memory
		t_client_info *client = (t_client_info*)ptr;
		close(client->fd);
		pthread_mutex_destroy(&client->network_mutex);
		pthread_mutex_destroy(&client->internal_mutex);
		sem_destroy(&client->response_sem);
		free(client);
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

void send_confirmation (uint32_t pid, int client_fd, pthread_mutex_t *mutex) { // Sends a confirmation package to the client, can be used to signal that a response is ready to be processed
	t_package *pkg = package_create();
	pkg->op_code = CONFIRMATION;
	package_add(pkg, &pid, sizeof(uint32_t));
	package_send(pkg, client_fd, mutex);
    package_delete(pkg);

	log_debug(logger, "Enviada confirmación para PID %d al cliente con fd %d", pid, client_fd);
}

void wait_confirmation (int client_fd) { // Waits for a confirmation package from the client, shouldnt be used because of busy waiting
	int op_code = operation_receive(client_fd);
	if (op_code != CONFIRMATION) {
		log_error(logger, "Se esperaba CONFIRMATION, se recibió: %d", op_code);
	}
}

void t_io_type_send (int server_fd, t_io_type module_type, t_log *logger, pthread_mutex_t *mutex) { // Sends a t_io_type to the server as part of the handshake process
    t_package *pkg = package_create();
    pkg->op_code = HANDSHAKE;  // Set to HANDSHAKE instead of PACKAGE
    package_add(pkg, &module_type, sizeof(t_io_type));
    package_send(pkg, server_fd, mutex);
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
	memcpy(&size, buffer + *offset, sizeof(int));
	*offset += sizeof(int);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}

char *bytes_to_safe_string(void *data, uint32_t size) {
	unsigned char *bytes = (unsigned char *)data;

	// check if the whole buffer is printable text (e.g. STDIN/STDOUT strings)
	bool all_printable = true;
	for (uint32_t i = 0; i < size; i++) {
		if (bytes[i] < 32 || bytes[i] > 126) {
			all_printable = false;
			break;
		}
	}

	if (all_printable) {
		// plain text: copy as-is and null-terminate
		char *result = malloc(size + 1);
		memcpy(result, bytes, size);
		result[size] = '\0';
		return result;
	}

	// binary data of a known register size: show as a clean decimal number
	if (size == 1) {
		uint8_t value;
		memcpy(&value, bytes, sizeof(value));
		char *result = malloc(4); // max "255" + '\0'
		sprintf(result, "%u", value);
		return result;
	}

	if (size == 4) {
		uint32_t value;
		memcpy(&value, bytes, sizeof(value));
		char *result = malloc(11); // max "4294967295" + '\0'
		sprintf(result, "%u", value);
		return result;
	}

	// any other size (or mixed printable/non-printable content): fall back to hex escapes
	char *result = malloc(size * 4 + 1);
	int pos = 0;
	for (uint32_t i = 0; i < size; i++) {
		if (bytes[i] >= 32 && bytes[i] <= 126) {
			result[pos++] = bytes[i];
		} else {
			pos += sprintf(result + pos, "\\x%02X", bytes[i]);
		}
	}
	result[pos] = '\0';
	return result;
}
