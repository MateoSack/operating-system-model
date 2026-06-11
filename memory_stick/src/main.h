#include <utils/server_utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

t_log *start_logger(t_config *config);
int server_setup(t_log *logger, int kernel_memory_fd);
int kernel_memory_handler(t_log *logger, t_config *config);
void *kernel_memory_thread(void *arg);
void *cpu_handler(void *fd_ptr);
void handle_read(int fd, pthread_mutex_t *net_mutex);
void handle_write(int fd, pthread_mutex_t *net_mutex);