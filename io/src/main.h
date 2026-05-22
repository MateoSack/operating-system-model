#include <utils/net_utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>

void end_program(int, t_log*, t_config*);
t_log *start_logger(t_config *config);
char *io_stdin(uint32_t pid, uint32_t size);
void io_stdout(uint32_t pid,char *output);
void io_sleep_ms(uint32_t pid,uint32_t ms); 