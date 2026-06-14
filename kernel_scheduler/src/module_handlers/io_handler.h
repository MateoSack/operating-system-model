#ifndef IO_HANDLER_H
#define IO_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>
#include <utils.h>
#include <managers/io_manager.h>

extern pthread_mutex_t io_id_mutex;

extern pthread_mutex_t pending_io_stdin_reading_mutex;

extern t_list *list_io_sleep;
extern t_list *list_io_stdin;
extern t_list *list_io_stdout;

extern t_list *pending_request_io_sleep;
extern t_list *pending_request_io_stdin;
extern t_list *pending_io_stdin_reading;
extern t_list *pending_request_io_stdout;

extern t_log *logger;

extern uint32_t next_io_id;

void io_handler (int io_fd);

#endif
