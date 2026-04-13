#include <utils/net_utils.h>

void add_client_to_list (t_list *list, int client_fd, int id);
void remove_client_from_list (t_list *list, t_client_info *client);
int id_assigner (int *next_client_id, int client_fd);
t_module_id handshake_receiver (int client_fd);
int server_start (t_log *logger);
int server_client_wait (int socket_server);