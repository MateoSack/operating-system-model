#include <commons/collections/list.h>
#include <utils/server_utils.h>

extern t_list *list_cpu;

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
        
    }
    return -1;
}
