#ifndef UTILS_NET_UTILS_H_
#define UTILS_NET_UTILS_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>

#define PORT "4444"

typedef enum {
	MESSAGE,
	PACKAGE,
    HANDSHAKE
} op_code;

typedef struct {
    int fd;
    int id;
} t_client_info;

typedef enum {
    MODULE_KERNEL_SCHEDULER,
    MODULE_CPU,
    MODULE_IO,
    MODULE_SWAP,
    MODULE_MEMORY_STICK
} t_module_id;


typedef struct {
	int size;
	void* stream;
} t_buffer;

typedef struct {
	op_code op_code;
	t_buffer* buffer;
} t_package;

typedef enum {
    NEW,
    READY,
    EXEC,
    BLOCK,
    SUSP_BLOCK,
    SUSP_READY,
    EXIT,
} t_process_state;

typedef struct {
    uint32_t pc;
	uint8_t ax;
	uint8_t bx;
    uint8_t cx;
    uint8_t dx;
	uint32_t eax;
	uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t si;
    uint32_t di;
} t_cpu_context;

typedef struct {
    uint32_t pid;
    t_process_state state;
    t_cpu_context context;
} t_pcb;

extern t_log* logger;

int server_start (t_log *logger);
int server_client_wait (int socket_server);
int connection_create (char *ip, char *port, t_log *logger);
void connection_liberate (int client_socket);
int operation_receive (int client_socket);
void *buffer_receive (int *size, int client_socket);
void message_receive (int client_socket);
void message_send (char *message, int client_socket);
void buffer_create (t_package *package);
t_package *package_create (void);
void package_add (t_package *package, void *value, int size);
void package_send (t_package *package, int client_socket);
void package_delete (t_package *package);
void pcb_handle (t_pcb *pcb, int client_socket);
uint32_t int32_deserialize(void *buffer, int *offset);
uint8_t int8_deserialize(void *buffer, int *offset);
t_pcb *pcb_receive(int socket_cliente);
void *package_serialize(t_package *package, int bytes);

#endif
