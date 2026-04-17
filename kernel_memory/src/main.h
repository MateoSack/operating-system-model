#include <commons/collections/list.h>
#include <utils/server_utils.h>
#include <commons/config.h>

void *handle_module(void *fd_ptr);
t_log *start_logger(t_config *config);
void update_cpu_list(t_list *list_cpu, t_module_credentials *new_credentials);
t_module_credentials *memory_stick_protocol(t_log *logger, int client_fd);
void memory_stick_handler (t_log *logger, int client_fd, t_module_credentials *client);