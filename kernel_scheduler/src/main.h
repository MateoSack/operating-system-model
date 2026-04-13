#include <utils/net_utils.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

void *client_handler_selector(void *fd_ptr);
void first_connection_with_kernel_memory (int kernel_memory_fd);
t_module_id handshake_receiver (int fd_client);
void cpu_handler (int cpu_fd);
void io_handler (int io_fd);
int id_assigner (t_module_id module_type, int client_fd);
void add_client_to_list (t_module_id module_type, int client_fd, int id);