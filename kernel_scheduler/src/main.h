#include <utils.h>
#include <long_term_scheduler.h>

int kernel_memory_handler (t_log *logger, t_config *config);
void *kernel_memory_thread ();
void *client_handler_selector(void *fd_ptr);
void cpu_handler (int cpu_fd);
void io_handler (int io_fd);
t_log *start_logger(t_config *config);