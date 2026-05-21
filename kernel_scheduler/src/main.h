#ifndef MAIN_H
#define MAIN_H

#include <utils.h>
#include <long_term_scheduler.h>
#include "module_handlers/kernel_memory_handler.h"
#include "module_handlers/cpu_handler.h"
#include "module_handlers/io_handler.h"

int kernel_memory_connection (t_log *logger, t_config *config, char *process0);
void *client_handler_selector(void *fd_ptr);
t_log *start_logger(t_config *config);

#endif
