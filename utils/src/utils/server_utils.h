#include <utils/net_utils.h>

void add_client_to_list (t_list *list, int client_fd, uint32_t id);
void remove_client_from_list (t_list *list, t_client_info *client);
uint32_t id_assigner (uint32_t *next_client_id, int client_fd);
t_module_id handshake_receiver (int client_fd);
int server_start (char *port, t_log *logger);
int server_client_wait (int socket_server);
char* get_port_from_fd (int fd, t_log *logger);
char* get_ip_from_fd (int fd, t_log *logger);
