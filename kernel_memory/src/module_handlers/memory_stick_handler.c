#include <commons/collections/list.h>
#include <utils/server_utils.h>

extern t_list *list_memory_stick;
extern t_list *list_memory_stick_credentials;

int memory_stick_handler (t_log *logger, int client_fd, t_module_credentials *client){
    while (1) {
        int op = operation_receive(client_fd);
        if (op == -1) {
            t_client_info *ms = malloc(sizeof(t_client_info));
	        ms->fd = client_fd;
	        ms->id = client->id;
            remove_client_from_list(list_memory_stick, ms);
            free(ms);
            list_remove_element(list_memory_stick_credentials, client);
            
            log_error(logger, "Module %d disconnected", client->id);

            free(client);
            //Llamar función de fallo y shutdown
            break;
        }
    }
    return -1;
}