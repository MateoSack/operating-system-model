#include <utils/net_utils.h>

t_module_id handshake_receiver (int client_fd);
int server_start (char *port, t_log *logger);
int server_client_wait (int socket_server);
char* get_port_from_fd (int fd, t_log *logger);
char* get_ip_from_fd (int fd, t_log *logger);
