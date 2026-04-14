#include <utils/server_utils.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

void *client_handler_selector(void *fd_ptr);
void cpu_handler (int cpu_fd);
void io_handler (int io_fd);
t_log *start_logger(t_config *config);