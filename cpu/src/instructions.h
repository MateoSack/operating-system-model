#include <utils/net_utils.h>
#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<utils/process_utils.h>
#include<main.h>


void instructions_cicle(t_cpu_context *context, uint32_t pid);
t_instruction_type instruction_to_type(char *instruction_str);
char **decode_instruction(char *content);
void execute_instruction(char **decoded_instruction, t_cpu_context *context);
void *process_execution_handler(void *args);
bool check_if_register(char *operand);