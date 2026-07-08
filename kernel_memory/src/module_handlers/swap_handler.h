#ifndef SWAP_HANDLER_H
#define SWAP_HANDLER_H

#include <commons/log.h>
#include <utils/server_utils.h>

extern uint32_t swap_block_size;
extern uint32_t swap_total_size;

int swap_handler(t_log *logger, int swap_fd);

#endif