#include <utils.h>

void process_set_state (t_process *process, t_process_state state, t_log *logger) { // Set process state and log the transition
    log_info(logger, "## (%d) Pasa del estado <%s> al estado <%s>", process->pid, process_state_to_string(process->state), process_state_to_string(state));
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

void add_process_to_ready_queue (t_process *process) { // Add a process to the ready queue based on its priority and scheduling algorithm
    if (scheduler_algorithm == CMN) {
        int queue_index;

        if (process->priority >= queue_algorithms_count) {
            log_warning(logger, "Proceso %d tiene una prioridad (%d) mayor a la cantidad de colas de planificación (%d). Agregándolo a la última cola.", process->pid, process->priority, queue_algorithms_count);
            queue_index = queue_algorithms_count - 1; // If the priority is greater than the number of queues, assign it to the last queue
        } else {
            queue_index = process->priority;
        }

        add_process_to_list(ready_queue[queue_index], process);
    } else {
        add_process_to_list(ready_queue[0], process); // For FIFO and RR, we can use a single ready queue
    }
}

void remove_process_from_list (t_list *list_processes, t_process *process) { // Remove a process from the list of processes
    list_remove_element(list_processes, process);
}

void remove_process_from_ready_queue (t_process *process) { // Remove a process from the ready queue
    if (scheduler_algorithm == CMN) {
        int queue_index;

        if (process->priority >= queue_algorithms_count) {
            log_warning(logger, "Proceso %d tiene una prioridad (%d) mayor a la cantidad de colas de planificación (%d). Buscando en la última cola.", process->pid, process->priority, queue_algorithms_count);
            queue_index = queue_algorithms_count - 1; // If the priority is greater than the number of queues, search in the last queue
        } else {
            queue_index = process->priority;
        }

        remove_process_from_list(ready_queue[queue_index], process);
    } else {
        remove_process_from_list(ready_queue[0], process); // For FIFO and RR, we can use a single ready queue
    }
}

void destroy_list_of_processes (t_list *list) { // Destroys a list of processes, freeing their memory
    void _destroy_process(void *ptr) {
        t_process *process = (t_process*)ptr;
        free(process);
    }

    list_destroy_and_destroy_elements(list, _destroy_process);
}

int ready_queue_size () { // Get the total size of the ready queue(s) based on the scheduling algorithm
    if (scheduler_algorithm == CMN) {
        int total_size = 0;
        for (int i = 0; i < queue_algorithms_count; i++) {
            total_size += list_size(ready_queue[i]);
        }
        return total_size;
    } else {
        return list_size(ready_queue[0]); // For FIFO and RR, we can use a single ready queue
    }
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
        log_warning(logger, "Algoritmo de planificación desconocido: %s. Estableciendo FIFO.", str);
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

t_process *get_process_from_cpu (t_client_info *cpu) { // Get a process from the list of processes based on its assigned CPU, returns NULL if not found
    bool _process_cpu_coincides (void *ptr) {
        t_process *p = (t_process*)ptr;
        return p->cpu == cpu;
    }

    t_process *process = list_find(list_processes, _process_cpu_coincides);

    return process;
}

void evict_process (t_process *process, t_interrupt_reason reason) { // Evict a process from the CPU
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
    package_add(pkg, &reason, sizeof(t_interrupt_reason));
    package_send(pkg, cpu_fd);
    package_delete(pkg);

    //wait_confirmation(cpu_fd); //TODO: Implement confirmation with semaphores to avoid busy waiting and the posibility that the next operation may not necesarily be a CONFIRMATION
}

void evict_all_processes (t_interrupt_reason reason) {
    while (1) {
        pthread_mutex_lock(&scheduler_mutex);

        if (list_size(exec_processes) == 0) {
            pthread_mutex_unlock(&scheduler_mutex);
            break;
        }

        t_process *process = list_get(exec_processes, 0);

        remove_process_from_list(exec_processes, process);
        process_set_state(process, READY, logger);
        add_process_to_ready_queue(process);

        pthread_mutex_unlock(&scheduler_mutex);

        evict_process(process, reason);
    }
}

t_client_info *get_available_io_type (t_list *io_list) {
	t_client_info *io;

	bool io_is_available(void *ptr) {
		t_client_info *client = (t_client_info*) ptr;
		return client->is_available == true;
	}

	io = list_find(io_list, (void*)io_is_available);

	return io;
}

t_io_numeric_process *get_next_io_numeric_process_from_list(t_list *io_pending_list) { // Gets the next t_io_numeric_process from the list, returns NULL if the list is empty
    if (list_is_empty(io_pending_list)) return NULL;

    t_io_numeric_process *io_process = list_get(io_pending_list, 0);
    list_remove(io_pending_list, 0);

    return io_process;
}

t_io_string_process *get_next_io_string_process_from_list(t_list *io_pending_list) { // Gets the next t_io_string_process from the list, returns NULL if the list is empty
    if (list_is_empty(io_pending_list)) return NULL;

    t_io_string_process *io_process = list_get(io_pending_list, 0);
    list_remove(io_pending_list, 0);

    return io_process;
}
