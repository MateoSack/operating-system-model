#include <commons/collections/list.h>
#include <utils/server_utils.h>
#include <commons/config.h>

void *handle_module(void *fd_ptr);
t_log *start_logger(t_config *config);
void update_cpu_list (t_list *list_cpu, t_list *list_memory_stick_credentials);