#include "cpu_handler.h"

extern t_list *list_cpu;
extern t_list *list_processes;

uint32_t pid_buscado;

int cpu_handler (t_log *logger, int client_fd, int cpu_id){
    while (1) {
        int op = operation_receive(client_fd);
        if (op == -1) {
            t_client_info *cpu = malloc(sizeof(t_client_info));
	        cpu->fd = client_fd;
	        cpu->id = cpu_id;
            remove_client_from_list(list_cpu, cpu);
            free(cpu);
            
            log_error(logger, "CPU %d disconnected", cpu_id);

            //Llamar función de fallo y shutdown
            break;
        }

        switch (op) {
            case CONTEXT_TRANSFER: {
                uint32_t pid = pid_decode(client_fd);
                log_info(logger, "Received context transfer request for PID %d from CPU %d", pid, cpu_id);
                pid_buscado = pid;
                t_pcb *pcb = list_find(list_processes, find_by_pid);
                if (pcb == NULL) {
                    log_error(logger, "Process with PID %d not found", pid);
                    break;
                }
                pcb->context = *context_receive(client_fd);
                log_info(logger, "Context updated correctly for PID %d", pid);
                break;
            }

            case CONTEXT_SEEK: {
                uint32_t pid = pid_decode(client_fd);
                log_info(logger, "Received context seek request for PID %d from CPU %d", pid, cpu_id);
                pid_buscado = pid;
                t_pcb *pcb = list_find(list_processes, find_by_pid);
                if (pcb == NULL) {
                    log_error(logger, "Process with PID %d not found", pid);
                    break;
                }
                context_send(&pcb->context, client_fd);
                log_info(logger, "Context sent correctly for PID %d", pid);
                break;
            }

        }
    }
    return -1;
}

bool find_by_pid(void *element) {
    t_pcb *pcb = (t_pcb *) element;
    return pcb->pid == pid_buscado;
}

