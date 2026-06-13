#include <schedulers/short_term_scheduler.h>

void *short_term_scheduler_main (void *arg) { // Main function for the short-term scheduler
    while (1) {
        sem_wait(&short_term_scheduler_sem);

        log_debug(logger, "Scheduler woke: ready_queue_size=%d, cpus=%d", ready_queue_size(), list_size(list_cpu));

        if (!can_schedule_get()) {
            log_debug(logger, "Can't schedule right now");
            continue;
        }

        bool no_processes = false;
        bool no_cpus = false;

        while (1) {
            pthread_mutex_lock(&scheduler_mutex);
            
            t_process *process = get_next_process_to_execute();
            t_client_info *cpu = get_available_cpu();

            no_processes = (process == NULL);
            no_cpus = (cpu == NULL);

            if (!no_processes && no_cpus && scheduler_algorithm == CMN) {
                // If there are no CPUs available and we're using CMN, we can try to evict a lower priority process to free up a CPU
                t_process *lowest_priority_process = get_lowest_priority_process(exec_processes);

                if (lowest_priority_process != NULL && lowest_priority_process->effective_priority > process->effective_priority) {
                    t_client_info *cpu_to_evict = lowest_priority_process->cpu;
                    pthread_mutex_unlock(&scheduler_mutex);

                    evict_process(cpu_to_evict, PRIORITY_PREEMPTION, true);

                    log_info(logger, "## (%d) Prioridad: %d - Desalojado por cola más prioritaria por el proceso (%d) con prioridad %d", lowest_priority_process->pid, lowest_priority_process->effective_priority, process->pid, process->effective_priority);

                    pthread_mutex_lock(&scheduler_mutex);
                }
            }

            if (no_processes || no_cpus) {
                pthread_mutex_unlock(&scheduler_mutex);
                break;
            }

            remove_process_from_ready_queue(process); // Remove the process from the ready queue

            pthread_mutex_lock(&cpu->internal_mutex);
            cpu->is_available = false;
            pthread_mutex_unlock(&cpu->internal_mutex);

            process_set_state(process, EXEC, logger);
            process_set_cpu(process, cpu);

            if(scheduler_algorithm != FIFO) process->start_exec_time = temporal_gettime(system_timer);

            add_process_to_list(exec_processes, process);

            pthread_mutex_unlock(&scheduler_mutex);

            send_pid_to_execute(process->pid, cpu);
        }

        if (no_processes) log_debug(logger, "Sin procesos en READY");
        if (no_cpus) log_debug(logger, "No hay CPUs disponibles");
    }

    return NULL;
}

t_process *get_next_process_to_execute () { // Returns the next process to execute based on the scheduling algorithm
    t_process *process = NULL;

    if (ready_queue_size() == 0) return NULL;

    switch (scheduler_algorithm) {
        case FIFO:
            process = (t_process*) list_get(ready_queue[0], 0); // Get the first process in the list
            break;
        case RR:
            process = (t_process*) list_get(ready_queue[0], 0); // Get the first process in the list
            break;
        case CMN:
            process = get_highest_priority_process_from_ready_queue(); // Get the process with the highest priority from the ready queues
            break;
        default:
            log_error(logger, "Algoritmo de planificación desconocido");
            break;
    }

    return process;
}

t_client_info *get_available_cpu () { // Returns an available CPU from the list of CPUs
    t_client_info *cpu;

    bool cpu_is_available(void *ptr) {
        t_client_info *client = (t_client_info*) ptr;

        bool available;
        pthread_mutex_lock(&client->internal_mutex);
        available = client->is_available && !client->is_evicting; // A CPU is available if it's marked as available and it's not in the process of evicting a process
        pthread_mutex_unlock(&client->internal_mutex);

        return available;
    }

    cpu = list_find(list_cpu, (void*)cpu_is_available);

    return cpu;
}

void send_pid_to_execute (uint32_t pid, t_client_info *cpu) { // Sends the process execution information to the CPU
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_EXECUTE;
    package_add(pkg, &pid, sizeof(uint32_t));

    package_send(pkg, cpu->fd, &cpu->network_mutex);

    package_delete(pkg);

    log_debug(logger, "Enviado proceso %d a CPU (fd: %d)", pid, cpu->fd);
}

void *quantum_manager (void *arg) { // Manages the quantum expiration for processes in the EXEC state
    int time_to_sleep = (quantum * 1000) / 50; // Sleep for a fraction of the quantum to check for expirations more frequently
    if (time_to_sleep < 1000) time_to_sleep = 1000; // Sleep at least 1 ms to avoid busy waiting in very low quantum scenarios

    while (1) {
        usleep(time_to_sleep);
        
        pthread_mutex_lock(&scheduler_mutex);
        
        bool found = true;
        while (found) {
            found = false;
            for (int i = 0; i < list_size(exec_processes); i++) {
                t_process *process = list_get(exec_processes, i);
                
                if (!process_has_quantum(process)) continue;
                
                uint64_t elapsed = temporal_gettime(system_timer) - process->start_exec_time;
                
                if (elapsed >= quantum && process->state == EXEC && process->cpu != NULL && !process->cpu->is_evicting) {
                    found = true;
                    t_client_info *cpu = process->cpu;
                    pthread_mutex_unlock(&scheduler_mutex);
                    evict_process(cpu, QUANTUM_EXPIRED, true);
                    log_info(logger, "## (%d) - Desalojado por fin de quantum", process->pid);
                    pthread_mutex_lock(&scheduler_mutex);
                    break;
                }
            }
        }
        
        pthread_mutex_unlock(&scheduler_mutex);
    }

    return NULL;
}
