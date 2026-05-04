#ifndef UTILS_NET_UTILS_H_
#define UTILS_NET_UTILS_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<commons/string.h>
#include<string.h>
#include<assert.h>
#include<pthread.h>
#include<arpa/inet.h>

typedef enum {
	MESSAGE,
	PACKAGE,
    HANDSHAKE,
    CREDENTIALS_UPDATE,
    PROCESS_CREATE,
    PROCESS_END,
    CONTEXT_TRANSFER,
    CONTEXT_SEEK,
    STATE_UPDATE,
    IO_MEMORY_READ,
    IO_MEMORY_WRITE,
} op_code;

typedef struct {
    int fd;
    uint32_t id;
} t_client_info;

typedef enum {
    MODULE_KERNEL_SCHEDULER,
    MODULE_CPU,
    MODULE_IO,
    MODULE_SWAP,
    MODULE_MEMORY_STICK
} t_module_id;

typedef struct {
    char *ip;
    char *port;
    uint32_t id;
} t_module_credentials;

typedef struct {
	int size;
	void *stream;
} t_buffer;

typedef struct {
	op_code op_code;
	t_buffer *buffer;
} t_package;

extern t_log *logger;

int connection_create (char *ip, char *port, t_log *logger);
void connection_liberate (int client_socket);
int operation_receive (int client_socket);
void *buffer_receive (int *size, int client_socket);
char *message_receive (t_log *logger, int client_socket);
void message_send (char *message, int client_socket);
void buffer_create (t_package *package);
t_package *package_create (void);
void package_add (t_package *package, void *value, int size);
void package_send (t_package *package, int client_socket);
void package_delete (t_package *package);
uint32_t uint32_deserialize(void *buffer, int *offset);
uint8_t uint8_deserialize(void *buffer, int *offset);
t_module_id t_module_id_deserialize(void *buffer, int *offset);
void *package_serialize(t_package *package, int bytes);
void t_module_id_send (int server_fd, t_module_id module_id, t_log *logger);
t_module_id t_module_id_receive (int client_fd);
uint32_t uint32_receive (int client_fd);
void uint32_send (int client_fd, uint32_t value);
void send_credentials_list (int fd, t_list *list, t_log *logger);
t_list *receive_credentials_list (int socket_cliente);
void send_credentials (int fd, t_module_credentials *cred, t_log *logger);
t_module_credentials *receive_credentials (int socket_cliente);
void t_module_credentials_destroyer (void *ptr);
t_client_info *add_client_to_list (t_list *list, int client_fd, uint32_t id);
void remove_client_from_list (t_list *list, t_client_info *client);
uint32_t uint32_decode (int client_fd);

#endif
