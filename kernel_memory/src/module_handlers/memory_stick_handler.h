#ifndef MEMORY_STICK_HANDLER_H
#define MEMORY_STICK_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>
#include <commons/collections/list.h>
#include <utils.h>

extern t_client_info *kernel_scheduler;

extern t_list *list_memory_stick;
extern t_list *list_memory_stick_credentials;
extern pthread_mutex_t list_memory_stick_mutex;
extern uint32_t total_memory_size;
extern pthread_mutex_t total_memory_size_mutex;

int memory_stick_handler(t_log *logger, int client_fd, t_module_credentials *client);

#endif
