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
            uint32_t pid = pid_decode(client_fd);
            char *relative_path = message_receive(logger, client_fd);
            
            // Construir path completo: base_path + relative_path
            char *full_path = string_from_format("%s/%s", base_path, relative_path);
            free(relative_path);
            log_info(logger, "Creating process %u with instructions in: %s", pid, full_path);
            
            t_pcb *pcb = create_pcb(pid, full_path);
            list_add(list_processes, pcb);
            
            break;
            }

            case PROCESS_END: {
                uint32_t pid = pid_decode(client_fd);
                log_info(logger, "Ending process PID:%u", pid);
                target_pid = pid;
                t_pcb *pcb_to_remove = list_find(list_processes, find_by_pid);
                if (pcb_to_remove != NULL) {
                    // A IMPLEMENTAR: Liberar recursos del proceso (segmentos, etc)
                    list_remove_element(list_processes, pcb_to_remove);
                    free(pcb_to_remove);
                    log_info(logger, "Process PID:%u ended correctly", pid);
                } else {
                    log_warning(logger, "Process PID:%u not found in list", pid);
                }
                
                break;
            }
        }
    }
    return -1;
}
