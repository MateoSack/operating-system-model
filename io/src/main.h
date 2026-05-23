#ifndef MAIN_H
#define MAIN_H

#include <utils/process_utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>

const t_io_type io_type_from_string(const char *str);
void handle_operation(int client_fd, t_io_type io_type);
char *io_stdin(uint32_t pid, uint32_t size);
void io_stdout(uint32_t pid,char *output);
void io_sleep_ms(uint32_t pid,uint32_t ms); 
t_log *start_logger(t_config *config);

#endif
