#include <utils/net_utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>

t_log *start_logger(t_config *config);
void send_swap_info(t_client_info *kernel_memory, uint32_t block_size, uint32_t swap_file_size);
void handle_block_read(int client_fd, FILE *swap_file, uint32_t block_size, pthread_mutex_t *write_mutex);
void handle_block_write(int client_fd, FILE *swap_file, uint32_t block_size, pthread_mutex_t *write_mutex);