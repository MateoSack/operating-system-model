#include <utils/net_utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>

void end_program(int, t_log*, t_config*);
t_log *start_logger(t_config *config);
int connect_kernel_memory_ (t_log *logger, t_config *config);
int connect_kernel_scheduler(t_log *logger, t_config *config);
void *kernel_memory_thread ();
void *kernel_scheduler_thread ();
