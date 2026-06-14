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
#include<semaphore.h>

typedef enum {
	MESSAGE,
	PACKAGE,
    HANDSHAKE,
    CONFIRMATION,
    CREDENTIALS_UPDATE,
    PROCESS_CREATE,
    PROCESS_END,
    PROCESS_EVICT,
    PROCESS_EXECUTE,
    PROCESS_INTERRUPTED, // Habria que ver de unificar las interrupciones como un solo código de operación y mandar un campo extra con el motivo
    CONTEXT_TRANSFER,
    CONTEXT_SEEK,
    STATE_UPDATE,
    IO_MEMORY_READ,
    IO_MEMORY_WRITE,
    IO_PROCESS,
    MEM_ALLOC,
    MEM_FREE,
    SLEEP,
    STDOUT,
    STDIN,
    INSTRUCTION_FETCH,
    MUTEX_CREATE,
    MUTEX_LOCK,
    MUTEX_UNLOCK,
    CORRUPTED_MEMORY,
    MEMORY_UPDATE,
    SEGMENT_CREATE,
    SEGMENT_DELETE,
    SEGMENT_RESULT,
    SEGMENTATION_FAULT,
    COMPACTION_REQUEST,
    COMPACTION_READY,
    COMPACTION_FINISHED,
    MS_READ,
    MS_WRITE,
    MS_READ_RESPONSE, // Ver de unificar con MS_READ
    MS_WRITE_OK,
} op_code;

typedef struct {
    int fd;
    uint32_t id;
    bool is_available;
    bool is_evicting; // Used to signal that the client is in the process of evicting a process and should not be assigned a new one until the eviction is confirmed
    pthread_mutex_t internal_mutex;
    pthread_mutex_t network_mutex;
    sem_t response_sem; // Used to signal the client handler thread that a response has been received and is ready to be processed
} t_client_info;

typedef enum {
    MODULE_KERNEL_SCHEDULER,
    MODULE_CPU,
    MODULE_IO,
    MODULE_SWAP,
    MODULE_MEMORY_STICK
} t_module_id;

typedef enum {
    IO_TYPE_STDIN,
    IO_TYPE_STDOUT,
    IO_TYPE_SLEEP,
} t_io_type;

typedef struct {
    char *ip;
    char *port;
    uint32_t id;
    uint32_t size;
} t_memory_stick_credentials;

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
char *message_decode (int client_socket);
void message_send (char *message, int client_socket, pthread_mutex_t *mutex);
void message_send_with_op_code (char *message, op_code op_code, int client_socket, pthread_mutex_t *mutex);
void buffer_create (t_package *package);
t_package *package_create (void);
void package_add (t_package *package, void *value, int size);
void package_string_add(t_package *package, char *message);
void package_send (t_package *package, int client_socket, pthread_mutex_t *mutex);
void package_delete (t_package *package);
char *string_deserialize(void *buffer, int *offset);
uint32_t uint32_deserialize(void *buffer, int *offset);
uint8_t uint8_deserialize(void *buffer, int *offset);
t_module_id t_module_id_deserialize(void *buffer, int *offset);
void *package_serialize(t_package *package, int bytes);
void t_module_id_send (int server_fd, t_module_id module_id, t_log *logger, pthread_mutex_t *mutex);
t_module_id t_module_id_decode (int client_fd);
uint32_t uint32_receive (int client_fd);
void uint32_send (int client_fd, uint32_t value, pthread_mutex_t *mutex);
void send_credentials_list (int fd, t_list *list, t_log *logger, pthread_mutex_t *mutex);
t_list *receive_credentials_list (int socket_cliente);
void send_credentials (int fd, t_memory_stick_credentials *cred, t_log *logger, pthread_mutex_t *mutex);
t_memory_stick_credentials *receive_credentials (int socket_cliente);
void t_memory_stick_credentials_destroyer (void *ptr);
t_client_info *add_client_to_list (t_list *list, int client_fd, uint32_t id);
t_client_info *create_client_info (int client_fd, uint32_t id);
void destroy_client(void *ptr);
void remove_client_from_list (t_list *list, t_client_info *client);
uint32_t uint32_decode (int client_fd);
void send_confirmation (uint32_t pid, int client_fd, pthread_mutex_t *mutex);
void wait_confirmation (int client_fd);
void t_io_type_send (int server_fd, t_io_type module_type, t_log *logger, pthread_mutex_t *mutex);
t_io_type t_io_type_receive (int client_fd);
t_io_type t_io_type_deserialize(void *buffer, int *offset);

#endif
