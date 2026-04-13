#include <utils/net_utils.h>

void add_client_to_list (t_list *list, int client_fd, int id);
int id_assigner (int *next_client_id, int client_fd);
t_module_id handshake_receiver (int client_fd);