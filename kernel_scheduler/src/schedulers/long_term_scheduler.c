#include <schedulers/long_term_scheduler.h>

int long_term_scheduler (char *path, uint8_t priority) {
    // TODO: Convert to thread and use a new list
    // TODO: Should check if can schedule

    pthread_mutex_lock(&scheduler_mutex);
    uint32_t pid = pid_assigner(&current_max_pid);
    pthread_mutex_unlock(&scheduler_mutex);

    log_info(logger, "## (%d) Se crea el proceso - Estado: NEW", pid);
    send_process_create_info(pid, path, kernel_memory->fd, &kernel_memory->network_mutex);
    free(path);

    t_process *process = create_process(pid, priority);

    pthread_mutex_lock(&scheduler_mutex);
    add_process_to_list(list_processes, process);
    pthread_mutex_unlock(&scheduler_mutex);

    return EXIT_SUCCESS;
}

void wait_process_create_confirmation (uint32_t pid) {
    pthread_mutex_lock(&scheduler_mutex);

    t_process *process = get_process_from_pid(pid);

    if (process == NULL) {
        pthread_mutex_unlock(&scheduler_mutex);
        log_error(logger, "No se encontró el proceso con PID %d para agregarlo a la cola de ready después de la confirmación de creación", pid);
        return;
    }

    process_set_state(process, READY, logger);

    add_process_to_ready_queue(process);
    log_debug(logger, "Proceso %d agregado a READY (ready_queue_size=%d)", process->pid, ready_queue_size());

    pthread_mutex_unlock(&scheduler_mutex);

    sem_post(&short_term_scheduler_sem);
}

uint32_t pid_assigner (uint32_t *current_pid) {
    uint32_t assigned_pid = *current_pid;
    (*current_pid)++;
    return assigned_pid;
}
