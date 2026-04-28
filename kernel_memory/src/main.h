#include <commons/collections/list.h>
#include <utils/server_utils.h>
#include <commons/config.h>
#include "module_handlers/memory_stick_handler.h"
#include "module_handlers/cpu_handler.h"
#include "module_handlers/kernel_scheduler_handler.h"
#include "module_handlers/swap_handler.h"

extern t_config *config;

void *handle_module(void *fd_ptr);
t_log *start_logger(t_config *config);
void update_cpu_list(t_module_credentials *new_credentials);
t_module_credentials *memory_stick_protocol(t_log *logger, int client_fd);