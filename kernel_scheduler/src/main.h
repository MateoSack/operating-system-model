#ifndef MAIN_H
#define MAIN_H

#include <utils.h>
#include <module_handlers/kernel_memory_handler.h>
#include <module_handlers/cpu_handler.h>
#include <module_handlers/io_handler.h>

void *client_handler_selector(void *fd_ptr);
t_log *start_logger(t_config *config);
int setup (char *config_path);
void *shutdown_handler (void *arg);

#endif
