#include <managers/io_manager.h>

int sleep_syscall_manager (t_process *process, t_client_info *cpu, uint32_t sleep_time) { // Manages the sleep syscall for a process, sending it to an available IO device of type SLEEP
    pthread_mutex_lock(&scheduler_mutex);
    process_set_state(process, BLOCK, logger);
    remove_process_from_list(exec_processes, process);
    int pid = process->pid;
    pthread_mutex_unlock(&scheduler_mutex);

    evict_process(process, IO_REQUEST);

    pthread_mutex_lock(&cpu->internal_mutex);
    cpu->is_available = true;
    pthread_mutex_unlock(&cpu->internal_mutex);
    
    t_io_numeric_process *io_process = t_io_numeric_process_create(pid, sleep_time, IO_TYPE_SLEEP);

    pthread_mutex_lock(&io_mutex);
    t_client_info *io = get_available_io_type(list_io_sleep);

    if (io != NULL) {
        pthread_mutex_lock(&io->internal_mutex);
        io->is_available = false;
        pthread_mutex_unlock(&io->internal_mutex);
        pthread_mutex_unlock(&io_mutex);

        io_numeric_process_send(io_process, SLEEP, io->fd, &io->network_mutex);

        log_debug(logger, "Proceso %d enviado a IO SLEEP (fd: %d) para dormir por %d ms", pid, io->fd, sleep_time);
    } else {
        list_add(pending_request_io_sleep, io_process);
        pthread_mutex_unlock(&io_mutex);

        log_debug(logger, "No hay dispositivos IO de tipo SLEEP disponibles para procesar la solicitud de sleep del proceso %d. El proceso quedará bloqueado hasta que un dispositivo IO de tipo SLEEP esté disponible.", pid);
    }

    sem_post(&short_term_scheduler_sem);

    return EXIT_SUCCESS;
}

int stdin_syscall_manager (t_process *process, t_client_info *cpu, uint32_t base, uint32_t limit) { // Manages the stdin syscall for a process, sending it to an available IO device of type STDIN
    pthread_mutex_lock(&scheduler_mutex);
    process_set_state(process, BLOCK, logger);
    remove_process_from_list(exec_processes, process);
    int pid = process->pid;
    pthread_mutex_unlock(&scheduler_mutex);

    evict_process(process, IO_REQUEST);

    pthread_mutex_lock(&cpu->internal_mutex);
    cpu->is_available = true;
    pthread_mutex_unlock(&cpu->internal_mutex);
    
    int value = 10; // This value should come from Kernel Memory read operation, but since we dont have it yet, we will use a dummy value

    t_io_numeric_process *io_process = t_io_numeric_process_create(pid, value, IO_TYPE_STDIN);

    pthread_mutex_lock(&io_mutex);
    t_client_info *io = get_available_io_type(list_io_stdin);

    if (io != NULL) {
        pthread_mutex_lock(&io->internal_mutex);
        io->is_available = false;
        pthread_mutex_unlock(&io->internal_mutex);
        pthread_mutex_unlock(&io_mutex);

        io_numeric_process_send(io_process, STDIN, io->fd, &io->network_mutex);

        log_debug(logger, "Proceso %d enviado a IO STDIN (fd: %d)", pid, io->fd);
    } else {
        list_add(pending_request_io_stdin, io_process);
        pthread_mutex_unlock(&io_mutex);

        log_debug(logger, "No hay dispositivos IO de tipo STDIN disponibles para procesar la solicitud de stdin del proceso %d. El proceso quedará bloqueado hasta que un dispositivo IO de tipo STDIN esté disponible.", pid);
    }

    sem_post(&short_term_scheduler_sem);

    return EXIT_SUCCESS;
}

int stdout_syscall_manager (t_process *process, t_client_info *cpu, uint32_t base, uint32_t limit) { // Manages the stdout syscall for a process, sending it to an available IO device of type STDOUT
    pthread_mutex_lock(&scheduler_mutex);
    process_set_state(process, BLOCK, logger);
    remove_process_from_list(exec_processes, process);
    int pid = process->pid;
    pthread_mutex_unlock(&scheduler_mutex);

    evict_process(process, IO_REQUEST);

    pthread_mutex_lock(&cpu->internal_mutex);
    cpu->is_available = true;
    pthread_mutex_unlock(&cpu->internal_mutex);

    char *value = "10"; // This value should come from Kernel Memory read operation, but since we dont have it yet, we will use a dummy value

    t_io_string_process *io_process = t_io_string_process_create(pid, value, IO_TYPE_STDOUT);

    pthread_mutex_lock(&io_mutex);
    t_client_info *io = get_available_io_type(list_io_stdout);

    if (io != NULL) {
        pthread_mutex_lock(&io->internal_mutex);
        io->is_available = false;
        pthread_mutex_unlock(&io->internal_mutex);
        pthread_mutex_unlock(&io_mutex);

        io_string_process_send(io_process, STDOUT, io->fd, &io->network_mutex);

        log_debug(logger, "Proceso %d enviado a IO STDOUT (fd: %d)", pid, io->fd);
    } else {
        list_add(pending_request_io_stdout, io_process);
        pthread_mutex_unlock(&io_mutex);

        log_debug(logger, "No hay dispositivos IO de tipo STDOUT disponibles para procesar la solicitud de stdout del proceso %d. El proceso quedará bloqueado hasta que un dispositivo IO de tipo STDOUT esté disponible.", pid);
    }

    sem_post(&short_term_scheduler_sem);

    return EXIT_SUCCESS;
}
