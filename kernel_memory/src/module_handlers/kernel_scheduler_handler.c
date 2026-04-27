#include <commons/collections/list.h>
#include <utils/server_utils.h>

int kernel_scheduler_handler (t_log *logger, int client_fd){
    while (1) {
        int op = operation_receive(client_fd);
        if (op == -1) {
            log_error(logger, "Kernel Scheduler disconnected");
            //Llamar función de fallo y shutdown
            break;
        }
    }
    return -1;
}