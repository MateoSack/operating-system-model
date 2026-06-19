#include "memory_stick_handler.h"

int memory_stick_handler (t_log *logger, int client_fd, t_memory_stick_credentials *client){
    while (1) {
        int op = operation_receive(client_fd);
            if (op == -1) {
                log_warning(logger, "Memory stick %d desconectado", client->id);
                
                pthread_mutex_lock(&list_memory_stick_mutex);
                bool ms_match(void *ptr) {
                    t_memory_stick_info *mi = (t_memory_stick_info *) ptr;
                    return mi->id == client->id || mi->fd == client_fd;
                }

                t_memory_stick_info *ms_corrupted = list_find(list_memory_stick, (void *) ms_match);
                if (ms_corrupted != NULL) {
                    pthread_mutex_destroy(&ms_corrupted->mutex);
                    sem_destroy(&ms_corrupted->response_sem);
                    list_remove_element(list_memory_stick, ms_corrupted);
                }
                pthread_mutex_unlock(&list_memory_stick_mutex);

                if (ms_corrupted != NULL) {
                    // Adjust total memory size
                    pthread_mutex_lock(&total_memory_size_mutex);
                    if (total_memory_size >= ms_corrupted->size) total_memory_size -= ms_corrupted->size;
                    pthread_mutex_unlock(&total_memory_size_mutex);

                    // Notify Kernel Scheduler (if exists) memory corruption
                    if (kernel_scheduler != NULL && kernel_scheduler->fd != -1) {
                        t_package *pkg = package_create();
                        pkg->op_code = CORRUPTED_MEMORY;
                        package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
                        package_delete(pkg);
                    }

                    free(ms_corrupted);
                }

                list_remove_element(list_memory_stick_credentials, client);
                free(client);
                break;
            }
        
        switch (op) {
            case MS_WRITE_RESPONSE: {
                int size;
                int offset = 0;
                void *buffer = buffer_receive(&size, client_fd);
                bool result = bool_deserialize(buffer, &offset);
                pthread_mutex_lock(&list_memory_stick_mutex);
                bool find_by_fd(void *ptr) {
                    return ((t_memory_stick_info *)ptr)->fd == client_fd;
                }
                t_memory_stick_info *ms = list_find(list_memory_stick, find_by_fd);
                pthread_mutex_unlock(&list_memory_stick_mutex);

                if (ms != NULL) {
                    pthread_mutex_lock(&ms->mutex);
                    ms->last_op_result = MS_WRITE_RESPONSE;
                    pthread_mutex_unlock(&ms->mutex);
                    sem_post(&ms->response_sem);
                }
                break;
            }

            case MS_READ_RESPONSE: {
                pthread_mutex_lock(&list_memory_stick_mutex);
                bool find_by_fd2(void *ptr) {
                    return ((t_memory_stick_info *)ptr)->fd == client_fd;
                }
                t_memory_stick_info *msr = list_find(list_memory_stick, find_by_fd2);
                pthread_mutex_unlock(&list_memory_stick_mutex);

                if (msr != NULL) {
                    int response_size;
                    void *buffer = buffer_receive(&response_size, client_fd);
                    pthread_mutex_lock(&msr->mutex);
                    msr->last_read_buffer = buffer;
                    msr->last_read_size = response_size;
                    pthread_mutex_unlock(&msr->mutex);
                    sem_post(&msr->response_sem);
                }
                break;
            }
        }
    }
    return -1;
}