#include <utils.h>

void process_set_state (t_process *process, t_process_state state, t_log *logger) { // Set process state and log the transition
    log_info(logger, "## (%d) Transitions from state <%s> to state <%s>", process->pid, process_state_to_string(process->state), process_state_to_string(state));
    process->state = state;
}

void process_set_cpu (t_process *process, t_client_info *cpu) { // Set assigned CPU to the process
    process->cpu = cpu;
}

t_process *create_process (uint32_t pid, uint8_t priority) { // Create a new process with the given PID and priority, returns the created process
    t_process *process = malloc(sizeof(t_process));
    process->pid = pid;
    process->priority = priority;
    process->state = NEW;
    process->cpu = NULL;
    process->start_exec_time = 0;

    return process;
}

void add_process_to_list (t_list *list_processes, t_process *process) { // Add a process to the list of processes
    list_add(list_processes, process);
}

void remove_process_from_list (t_list *list_processes, t_process *process) { // Remove a process from the list of processes
    list_remove_element(list_processes, process);
}

void send_process_create_info (uint32_t pid, char *path, int kernel_memory_fd) { // Send the process creation information to Kernel Memory
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_CREATE;
    package_add(pkg, &pid, sizeof(uint32_t));
	package_send(pkg, kernel_memory_fd);
    package_delete(pkg);

    message_send(path, kernel_memory_fd);
}

t_scheduler_algorithm scheduler_algorithm_from_string(const char *str) { // Convert a string to the corresponding scheduler algorithm enum value
    if (strcmp(str, "FIFO") == 0) {
        return FIFO;
    } else if (strcmp(str, "RR") == 0) {
        return RR;
    } else if (strcmp(str, "CMN") == 0) {
        return CMN;
    } else {
        log_warning(logger, "Unknown scheduler algorithm: %s. Defaulting to FIFO.", str);
        return FIFO; // Default to FIFO if unknown
    }
}

t_process *get_process_from_pid (uint32_t pid) { // Get a process from the list of processes based on its PID, returns NULL if not found
    bool _process_pid_coincides (void *ptr) {
        t_process *p = (t_process*)ptr;
        return p->pid == pid;
    }

    t_process *process = list_find(list_processes, _process_pid_coincides);

    return process;
}

t_process *get_process_by_cpu (t_client_info *cpu) { // Get a process from the list of processes based on its assigned CPU, returns NULL if not found
    bool _process_cpu_coincides (void *ptr) {
        t_process *p = (t_process*)ptr;
        return p->cpu == cpu;
    }

    t_process *process = list_find(list_processes, _process_cpu_coincides);

    return process;
}

void evict_process (t_process *process) { // Evict a process from the CPU
    int cpu_fd = -1;

    pthread_mutex_lock(&scheduler_mutex);
    if (process->cpu != NULL) {
        cpu_fd = process->cpu->fd;
        process_set_cpu(process, NULL);
    }
    pthread_mutex_unlock(&scheduler_mutex);

    if (cpu_fd == -1) return;

    t_package *pkg = package_create();
    pkg->op_code = PROCESS_EVICT;
    package_send(pkg, cpu_fd);
    package_delete(pkg);

    //wait_confirmation(cpu_fd); //TODO: Implement confirmation with semaphores to avoid busy waiting and the posibility that the next operation may not necesarily be a CONFIRMATION
}

void evict_all_processes () {
    while (1) {
        pthread_mutex_lock(&scheduler_mutex);

        if (list_size(exec_processes) == 0) {
            pthread_mutex_unlock(&scheduler_mutex);
            break;
        }

        t_process *process = list_get(exec_processes, 0);

        remove_process_from_list(exec_processes, process);
        process_set_state(process, READY, logger);
        add_process_to_list(ready_queue, process);

        pthread_mutex_unlock(&scheduler_mutex);

        evict_process(process);
    }
}
