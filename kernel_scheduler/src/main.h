#include <utils/server_utils.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

int kernel_memory_handler (t_log *logger, t_config *config);
void *kernel_memory_thread ();
void *client_handler_selector(void *fd_ptr);
void cpu_handler (int cpu_fd);
void io_handler (int io_fd);
t_log *start_logger(t_config *config);