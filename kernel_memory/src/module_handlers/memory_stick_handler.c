#include <commons/collections/list.h>
#include <utils/server_utils.h>
#include <utils.h>

extern t_list *list_memory_stick;
extern t_list *list_memory_stick_credentials;
extern int kernel_scheduler_fd;
extern pthread_mutex_t kernel_scheduler_mutex;
extern pthread_mutex_t list_memory_stick_mutex;
extern uint32_t total_memory_size;
extern pthread_mutex_t total_memory_size_mutex;

int memory_stick_handler (t_log *logger, int client_fd, t_module_credentials *client){
    while (1) {
        int op = operation_receive(client_fd);
            if (op == -1) {
                
                pthread_mutex_lock(&list_memory_stick_mutex);
                bool ms_match(void *ptr) {
                    t_memory_stick_info *mi = (t_memory_stick_info *) ptr;
                    return mi->id == client->id || mi->fd == client_fd;
                }

                t_memory_stick_info *ms_corrupted = list_find(list_memory_stick, (void *) ms_match);
                if (ms_corrupted != NULL) {
                    list_remove_element(list_memory_stick, ms_corrupted);
                }
                pthread_mutex_unlock(&list_memory_stick_mutex);

                if (ms_corrupted != NULL) {
                    // Adjust total memory size
                    pthread_mutex_lock(&total_memory_size_mutex);
                    if (total_memory_size >= ms_corrupted->size) total_memory_size -= ms_corrupted->size;
                    pthread_mutex_unlock(&total_memory_size_mutex);

                    // Notify Kernel Scheduler (if exists) memory corruption
                    t_package *pkg = package_create();
                    pkg->op_code = CORRUPTED_MEMORY;
                    pthread_mutex_lock(&kernel_scheduler_mutex);
                    if (kernel_scheduler_fd != -1) package_send(pkg, kernel_scheduler_fd);
                    pthread_mutex_unlock(&kernel_scheduler_mutex);
                    package_delete(pkg);

                    free(ms_corrupted);
                }

                list_remove_element(list_memory_stick_credentials, client);
                log_error(logger, "Module %d disconnected", client->id);
                free(client);
                break;
            }
    }
    return -1;
}