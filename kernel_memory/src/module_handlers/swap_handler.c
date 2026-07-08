#include <commons/collections/list.h>
#include <utils/server_utils.h>
#include "memory_manage.h"

int swap_handler (t_log *logger, int swap_fd){
    swap_block_size = uint32_receive(swap_fd);
    swap_total_size = uint32_receive(swap_fd);
    log_debug(logger, "SWAP - block_size: %u, total_size: %u", swap_block_size, swap_total_size);
    while (1) {
        int op = operation_receive(swap_fd);
        if (op == -1) {
            log_error(logger, "Swap desconectado");
            //Llamar función de fallo y shutdown
            break;
        }
        switch (op) {
            case SWAP_IN: {
                int buf_size;
                void *block_buffer = buffer_receive(&buf_size, swap_fd);

                pthread_mutex_lock(&swap_read_response.mutex);
                if (swap_read_response.data != NULL) free(swap_read_response.data);
                swap_read_response.data  = block_buffer;
                swap_read_response.size  = buf_size;
                swap_read_response.ready = true;
                pthread_mutex_unlock(&swap_read_response.mutex);

                sem_post(&swap_read_response.sem); // Signal that the read operation is done
                break;
            }

            case SWAP_OUT: {
                sem_post(&sem_swap_write_done); // Signal that the write operation is done
                break;
            }

            default:
                log_warning(logger, "swap_handler: op desconocido %d", op);
                break;
        }
    }
    return -1;
}