#include <schedulers/medium_term_scheduler.h>

void *suspension_manager (void *arg) { // Manages the suspension_timeout expiration for processes in the BLOCK state
    int time_to_sleep = (suspension_timeout * 1000) / 50; // Sleep for a fraction of the suspension_timeout to check for expirations more frequently
    if (time_to_sleep < 1000) time_to_sleep = 1000; // Sleep at least 1 ms to avoid busy waiting in very low suspension_timeout scenarios

    while (1) {
        usleep(time_to_sleep);

        if (!can_schedule_get()) continue;

        t_list *to_suspend_list = list_create();

        pthread_mutex_lock(&scheduler_mutex);
        pthread_mutex_lock(&block_processes_mutex);

        for (int i = 0; i < list_size(block_processes); i++) {
            t_process *process = list_get(block_processes, i);

            uint64_t elapsed = temporal_gettime(system_timer) - process->start_block_time;

            if (process->state == BLOCK && elapsed >= suspension_timeout) {
                log_debug(logger, "## (%d) Se suspende proceso bloqueado", process->pid);

                list_remove_element(block_processes, process);

                i--; // So as to start from last position after removing one process from list

                process_set_state(process, SUSP_BLOCK, logger); // Optimistic state change, will be confirmed by kernel memory after SWAP_OUT
                list_add(to_suspend_list, process);
            }
        }

        pthread_mutex_unlock(&block_processes_mutex);
        pthread_mutex_unlock(&scheduler_mutex);

        for (int i = 0; i < list_size(to_suspend_list); i++) {
            t_process *process = list_get(to_suspend_list, i);

            log_debug(logger, "(%d) pedido de SWAP_OUT enviado a kernel memory", process->pid);
            uint32_send_with_op_code(kernel_memory->fd, process->pid, SWAP_OUT, &kernel_memory->network_mutex);
        }

        list_destroy(to_suspend_list);
    }

    return NULL;
}

void wait_confirmation_swap_out (uint32_t pid, bool ok) {
    t_process *process = get_process_from_pid(pid);

    if (process == NULL) {
        log_warning(logger, "## (%d) Se recibe confirmación de SWAP_OUT, pero el proceso no se encuentra en la lista de procesos", pid);
        return;
    }

    pthread_mutex_lock(&scheduler_mutex);
    t_process_state current = process->state; // should be SUSP_BLOCK or SUSP_READY

    if (current != SUSP_BLOCK && current != SUSP_READY) {
        pthread_mutex_unlock(&scheduler_mutex);
        log_warning(logger, "## (%d) Se recibe confirmación/rechazo de SWAP_OUT, pero el proceso no se encuentra en estado SUSP_BLOCK o SUSP_READY, estado = %d", pid, current);
        return;
    }

    if (!ok && current == SUSP_READY) {
        process_set_state(process, READY, logger);
        add_process_to_ready_queue(process);
        pthread_mutex_unlock(&scheduler_mutex);

        log_debug(logger, "## (%d) Se rechaza SWAP_OUT del proceso listo, volviendo a READY", pid);

        sem_post(&short_term_scheduler_sem);

        return;
    } else if (!ok && current == SUSP_BLOCK) {
        process_set_state(process, BLOCK, logger);
        process->start_block_time = temporal_gettime(system_timer);
        pthread_mutex_unlock(&scheduler_mutex);

        log_debug(logger, "## (%d) Se rechaza SWAP_OUT del proceso bloqueado, volviendo a BLOCK", pid);

        pthread_mutex_lock(&block_processes_mutex);
        list_add(block_processes, process);
        pthread_mutex_unlock(&block_processes_mutex);

        return;
    }
    pthread_mutex_unlock(&scheduler_mutex);
    
    // if ok==true, we don´t change the state as its already correct
    if (ok) {
        log_debug(logger, "## (%d) Se confirma SWAP_OUT del proceso bloqueado", pid);
        
        pthread_mutex_lock(&suspended_processes_mutex);
        list_add(suspended_processes, process);
        pthread_mutex_unlock(&suspended_processes_mutex);
    }
}

void request_swap_in () { // Requests a SWAP_IN for all processes in the SUSP_READY state, sending their PIDs to the memory manager
    bool _is_susp_ready (void *ptr) {
        t_process *process = (t_process*)ptr;
        return process->state == SUSP_READY;
    }

    pthread_mutex_lock(&suspended_processes_mutex);
    t_list *ready_to_swap = list_filter(suspended_processes, _is_susp_ready);
    t_list *sorted_list = sort_processes_by_priority(ready_to_swap);
    t_list *to_swap_in_list = process_list_to_pid_list(sorted_list);
    int suspended_processes_count = list_size(suspended_processes);
    pthread_mutex_unlock(&suspended_processes_mutex);

    if (list_size(to_swap_in_list) == 0) {
        log_debug(logger, "No hay procesos en SUSP_READY para hacer SWAP_IN");
    } else {
        log_debug(logger, "Solicitando SWAP_IN para %d de %d procesos suspendidos (filtrados por SUSP_READY)", list_size(to_swap_in_list), suspended_processes_count);

        uint32_list_send(kernel_memory->fd, &kernel_memory->network_mutex, to_swap_in_list, SWAP_IN);
    }

    list_destroy(ready_to_swap);
    list_destroy(sorted_list);
    list_destroy_and_destroy_elements(to_swap_in_list, free);
}

void wait_confirmation_swap_in (t_list *pid_list) { // Receives a list of PIDs that were successfully swapped in and updates their state accordingly
    bool new_ready_processes = false; // if any process was moved to READY state, we will signal the short_term_scheduler_sem at the end

    for (int i = 0; i < list_size(pid_list); i++) {
        uint32_t pid = *(uint32_t*)list_get(pid_list, i);
        t_process *process = get_process_from_pid(pid);

        if (process != NULL) {
            pthread_mutex_lock(&suspended_processes_mutex);
            list_remove_element(suspended_processes, process);
            pthread_mutex_unlock(&suspended_processes_mutex);

            if (process->state == SUSP_READY) {
                log_debug(logger, "Proceso %d en estado SUSP_READY al recibir confirmación de SWAP_IN, volviendo a READY", pid);
                new_ready_processes = true;
                pthread_mutex_lock(&scheduler_mutex);
                process_set_state(process, READY, logger);
                add_process_to_ready_queue(process);
                pthread_mutex_unlock(&scheduler_mutex);
            } else if (process->state == SUSP_BLOCK) {
                log_debug(logger, "Proceso %d en estado SUSP_BLOCK al recibir confirmación de SWAP_IN, volviendo a BLOCK", pid);
                pthread_mutex_lock(&scheduler_mutex);
                process_set_state(process, BLOCK, logger);
                pthread_mutex_unlock(&scheduler_mutex);
                pthread_mutex_lock(&block_processes_mutex);
                process->start_block_time = temporal_gettime(system_timer);
                add_process_to_list(block_processes, process);
                pthread_mutex_unlock(&block_processes_mutex);
            } else {
                log_warning(logger, "Proceso %d en estado inesperado (%d) al recibir confirmación de SWAP_IN", pid, process->state);
            }
        } else {
            log_warning(logger, "Proceso %d no encontrado al recibir confirmación de SWAP_IN", pid);
        }
    }

    list_destroy(pid_list);

    if (new_ready_processes) sem_post(&short_term_scheduler_sem);
}
