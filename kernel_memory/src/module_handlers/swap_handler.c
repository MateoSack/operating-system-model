#include <commons/collections/list.h>
#include <utils/server_utils.h>

int swap_handler (t_log *logger, int swap_fd){
    while (1) {
        int op = operation_receive(swap_fd);
        if (op == -1) {
            log_error(logger, "Swap disconnected");
            //Llamar función de fallo y shutdown
            break;
        }
    }
    return -1;
}