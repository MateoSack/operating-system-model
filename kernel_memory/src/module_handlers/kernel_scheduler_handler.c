#include "kernel_scheduler_handler.h"

extern t_list *list_processes;

int kernel_scheduler_handler(t_log *logger, int client_fd, t_config *config) {
    char *base_path = config_get_string_value(config, "SCRIPTS_BASEPATH");
    
    while (1) {
        int op = operation_receive(client_fd);
        if (op == -1) {
            log_error(logger, "Kernel Scheduler disconnected");
            break;
        }

        switch (op) {
        case PROCESS_CREATE: {
            uint32_t pid = uint32_receive(client_fd);
            char *relative_path = message_receive(logger, client_fd);
            
            // Construir path completo: base_path + relative_path
            char *full_path = string_from_format("%s/%s", base_path, relative_path);
            free(relative_path);
            log_info(logger, "Creating process %u with instructions in: %s", pid, full_path);
            
            t_pcb *pcb = create_pcb(pid, full_path);
            list_add(list_processes, pcb);
            
            break;
        }
        }
    }
    return -1;
}
