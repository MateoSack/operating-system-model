#include <utils/server_utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>

void end_program(int, t_log*, t_config*);
t_log *start_logger(t_config *config);
int server_setup (t_log *logger, int kernel_memory_fd);
int kernel_memory_handler (t_log *logger, t_config *config);
void *kernel_memory_thread ();
void *cpu_handler (void *fd_ptr);
