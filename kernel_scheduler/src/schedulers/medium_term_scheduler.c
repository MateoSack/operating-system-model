#include <schedulers/medium_term_scheduler.h>

void *suspension_manager (void *arg) { // Manages the suspension_timeout expiration for processes in the BLOCK state
    int time_to_sleep = (suspension_timeout * 1000) / 50; // Sleep for a fraction of the suspension_timeout to check for expirations more frequently
    if (time_to_sleep < 1000) time_to_sleep = 1000; // Sleep at least 1 ms to avoid busy waiting in very low suspension_timeout scenarios

    while (1) {
        usleep(time_to_sleep);

        if (!can_schedule_get()) continue;

        t_list *to_suspend_list = list_create();

        pthread_mutex_lock(&block_processes_mutex);

        for (int i = 0; i < list_size(block_processes); i++) {
            t_process *process = list_get(block_processes, i);

            uint64_t elapsed = temporal_gettime(system_timer) - process->start_block_time;

            if (process->state == BLOCK && elapsed >= suspension_timeout) {
                log_debug(logger, "## (%d) Se suspende proceso bloqueado", process->pid);

                list_remove_element(block_processes, process);

                i--; // So as to start from last position after removing one process from list

                list_add(to_suspend_list, process);
            }
        }

        pthread_mutex_unlock(&block_processes_mutex);

        for (int i = 0; i < list_size(to_suspend_list); i++) {
            t_process *process = list_get(to_suspend_list, i);

            uint32_send_with_op_code(kernel_memory->fd, process->pid, SWAP_OUT, &kernel_memory->internal_mutex);
        }

        list_destroy(to_suspend_list);
    }

    return NULL;
}

void wait_confirmation_swap_out (uint32_t pid, bool ok) {
    t_process *process = get_process_from_pid(pid);

    if (ok) {
        process_set_state(process, SUSP_BLOCK, logger);

        pthread_mutex_lock(&suspended_processes_mutex);
        list_add(suspended_processes, process);
        pthread_mutex_unlock(&suspended_processes_mutex);
    } else {
        pthread_mutex_lock(&block_processes_mutex);
        list_add(block_processes, process);
        pthread_mutex_unlock(&block_processes_mutex);
    }
}
