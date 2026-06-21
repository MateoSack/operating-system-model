#include "memory_manager.h"

void receive_instruction_mem_alloc (uint32_t *pid, uint32_t *segment_id, uint32_t *segment_size, int cpu_fd) {
	int size;
	int offset = 0;
	void *buffer = buffer_receive(&size, cpu_fd);
	if (buffer == NULL) return;

	*pid = uint32_deserialize(buffer, &offset);
	*segment_id = uint32_deserialize(buffer, &offset);
	*segment_size = uint32_deserialize(buffer, &offset);

	free(buffer);
}

void mem_alloc_syscall_manager(uint32_t pid, uint32_t segment_id, uint32_t segment_size) {
    t_process *process = get_process_from_pid(pid);

    if (process == NULL) {
        log_error(logger, "mem_alloc_syscall_manager: PID %u no encontrado", pid);
        return;
    }

    if (segment_size == 0) {
        log_error(logger, "mem_alloc_syscall_manager: tamaño de segmento no puede ser 0");
        return;
    }

    t_package *package = package_create();
    package->op_code = SEGMENT_CREATE;
    package_add(package, &pid, sizeof(uint32_t));
    package_add(package, &segment_id, sizeof(uint32_t));
    package_add(package, &segment_size, sizeof(uint32_t));

    package_send(package, kernel_memory->fd, &kernel_memory->network_mutex);
    package_delete(package);
}

void receive_instruction_mem_free (uint32_t *pid, uint32_t *segment_id, int cpu_fd) {
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, cpu_fd);
    if (buffer == NULL) return;

    *pid = uint32_deserialize(buffer, &offset);
    *segment_id = uint32_deserialize(buffer, &offset);

    free(buffer);
}

void mem_free_syscall_manager(uint32_t pid, uint32_t segment_id) {
    t_process *process = get_process_from_pid(pid);

    if (process == NULL) {
        log_error(logger, "mem_free_syscall_manager: PID %u no encontrado", pid);
        return;
    }

    t_package *package = package_create();
    package->op_code = SEGMENT_DELETE;
    package_add(package, &pid, sizeof(uint32_t));
    package_add(package, &segment_id, sizeof(uint32_t));

    package_send(package, kernel_memory->fd, &kernel_memory->network_mutex);
    package_delete(package);
}

void handle_segment_result (uint32_t pid, uint32_t segment_id, t_segment_result result) {
    t_process *process = get_process_from_pid(pid);

    t_client_info *cpu = process->cpu;

    if (result == SEGMENT_OK) {
        log_debug(logger, "Syscall de memoria exitosa sobre segmento %d", segment_id);
        send_pid_to_execute(process->pid, cpu);
    } else if (result == SEGMENT_NO_SPACE) {
        log_debug(logger, "No se pudo crear el segmento por falta de espacio. Terminando proceso");

        process_set_state(process, EXIT, logger);
        process_set_cpu(process, NULL);

        pthread_mutex_lock(&scheduler_mutex);
        cpu->is_available = true;
        pthread_mutex_unlock(&scheduler_mutex);
        sem_post(&short_term_scheduler_sem);
    } else {
        log_debug(logger, "Error en syscall de memoria. Terminando proceso");

        process_set_state(process, EXIT, logger);
        process_set_cpu(process, NULL);

        pthread_mutex_lock(&scheduler_mutex);
        cpu->is_available = true;
        pthread_mutex_unlock(&scheduler_mutex);
        sem_post(&short_term_scheduler_sem);
    }
}

void compaction_requested () {
    pthread_t thread;
    pthread_create(&thread, NULL, compaction_requested_thread, NULL);
    pthread_detach(thread);
}

void *compaction_requested_thread (void *arg) {
    log_info(logger, "## Inicio de compactación");

    can_schedule_write(false);

    evict_all_processes(COMPACTION);

    log_debug(logger, "Ya se han desalojado todos los procesos");

    t_package *package = package_create();
    package->op_code = COMPACTION_READY;
    package_send(package, kernel_memory->fd, &kernel_memory->network_mutex);
    package_delete(package);

    log_debug(logger, "Enviado COMPACTION_READY");

    sem_wait(&compaction_finished_sem);

    can_schedule_write(true);

    log_info(logger, "## Fin de compactación");

    sem_post(&short_term_scheduler_sem);

    return NULL;
}

void memory_corrupted () {
    pthread_t thread;
    pthread_create(&thread, NULL, memory_corrupted_thread, NULL);
    pthread_detach(thread);
}

void *memory_corrupted_thread (void *arg) {
    log_debug(logger, "Memoria corrupta, desalojando los procesos...");

    can_schedule_write(false);

    evict_all_processes(CORRUPT_MEMORY);

    log_debug(logger, "Todos los procesos desalojados. Apagando...");

    exit (EXIT_FAILURE);

    return NULL;
}

void memory_update (uint32_t new_size) {
    log_debug(logger, "Actualización de memoria recibida (Tamaño libre: %d), intentado traer procesos a memoria", new_size);

    // TODO: Send list of PIDs in swap to memory and wait for which ones were able to make it
}
