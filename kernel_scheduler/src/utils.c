#include <utils.h>

void process_set_state (t_process *process, t_process_state state, t_log *logger) { // Set process state and log the transition
    log_info(logger, "## (%d) Pasa del estado <%s> al estado <%s>", process->pid, process_state_to_string(process->state), process_state_to_string(state));
    process->state = state;
}

void process_set_cpu (t_process *process, t_client_info *cpu) { // Set assigned CPU to the process
    process->cpu = cpu;
}

t_process *create_process (uint32_t pid, uint8_t base_priority) { // Create a new process with the given PID and priority, returns the created process
    t_process *process = malloc(sizeof(t_process));
    process->pid = pid;
    process->base_priority = base_priority;
    process->effective_priority = base_priority;
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

        if (process->effective_priority >= queue_algorithms_count) {
            log_warning(logger, "Proceso %d tiene una prioridad efectiva (%d) mayor a la cantidad de colas de planificación (%d). Agregándolo a la última cola.", process->pid, process->effective_priority, queue_algorithms_count);
            queue_index = queue_algorithms_count - 1; // If the priority is greater than the number of queues, assign it to the last queue
        } else {
            queue_index = process->effective_priority;
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

        if (process->effective_priority >= queue_algorithms_count) {
            log_warning(logger, "Proceso %d tiene una prioridad efectiva (%d) mayor a la cantidad de colas de planificación (%d). Buscando en la última cola.", process->pid, process->effective_priority, queue_algorithms_count);
            queue_index = queue_algorithms_count - 1; // If the priority is greater than the number of queues, search in the last queue
        } else {
            queue_index = process->effective_priority;
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

void send_process_create_info (uint32_t pid, char *path, int kernel_memory_fd, pthread_mutex_t *mutex) { // Send the process creation information to Kernel Memory
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_CREATE;
    package_add(pkg, &pid, sizeof(uint32_t));
	package_send(pkg, kernel_memory_fd, mutex);
    package_delete(pkg);

    message_send(path, kernel_memory_fd, mutex);
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

bool process_has_quantum (t_process *process) { // Check if a process has quantum assigned based on the scheduling algorithm
    if (scheduler_algorithm == RR) return true;
    if (scheduler_algorithm == FIFO) return false;

    bool has_quantum = false;

    int priority = process->effective_priority;

    if (priority >= queue_algorithms_count) {
        log_warning(logger, "Proceso %d tiene una prioridad efectiva (%d) mayor a la cantidad de colas de planificación (%d). Usando última cola.", process->pid, process->effective_priority, queue_algorithms_count);
        priority = queue_algorithms_count - 1; // If the priority is greater than the number of queues, assume it has quantum assigned according to the last queue
    }

    has_quantum = queue_algorithms[priority] == RR;

    return has_quantum;
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

t_process *get_highest_priority_process_from_ready_queue () { // Get the highest priority process from the ready queue, returns NULL if the ready queue is empty. Use under mutex
    t_process *highest_priority_process = NULL;

    for (int i = 0; i < queue_algorithms_count; i++) {
        if (!list_is_empty(ready_queue[i])) {
            highest_priority_process = list_get(ready_queue[i], 0); // Get the first process in the queue, which is the highest priority one
            break;
        }
    }

    return highest_priority_process;
}

t_process *get_lowest_priority_process (t_list *process_list) { // Get the lowest priority process from a list of processes, returns NULL if the list is empty. Use under mutex
    if (list_is_empty(process_list)) return NULL;

    t_process *lowest_priority_process = list_get(process_list, 0);

    for (int i = 1; i < list_size(process_list); i++) {
        t_process *current_process = list_get(process_list, i);
        if (current_process->effective_priority > lowest_priority_process->effective_priority) { // 0 is the highest priority, so we look for the process with the greatest priority value
            lowest_priority_process = current_process;
        }
    }

    return lowest_priority_process;
}

void evict_process(t_client_info *cpu, t_interrupt_reason reason) {
    pthread_mutex_lock(&cpu->internal_mutex);
    cpu->is_evicting = true;
    pthread_mutex_unlock(&cpu->internal_mutex);

    t_package *pkg = package_create();
    pkg->op_code = PROCESS_EVICT;
    package_add(pkg, &reason, sizeof(t_interrupt_reason));
    package_send(pkg, cpu->fd, &cpu->network_mutex);
    package_delete(pkg);

    pthread_t thread;
    pthread_create(&thread, NULL, wait_confirmation_thread, (void*)cpu);
    pthread_detach(thread);
}

void *wait_confirmation_thread (void *arg) { // Wait for a confirmation from the CPU that the process was successfully evicted and is ready to be sent to the ready queue
    t_client_info *cpu = (t_client_info*)arg;

    sem_wait(&cpu->response_sem);

    log_debug(logger, "Confirmación de evict recibida para CPU %d", cpu->id);

    pthread_mutex_lock(&scheduler_mutex);
    t_process *process = get_process_from_cpu(cpu);
    if (process != NULL) {
        remove_process_from_list(exec_processes, process);
        process_set_state(process, READY, logger);
        add_process_to_ready_queue(process);
        process_set_cpu(process, NULL);
        pthread_mutex_lock(&cpu->internal_mutex);
        cpu->is_available = true;
        cpu->is_evicting = false; // Mark the CPU as not evicting anymore so it can be assigned a new process
        pthread_mutex_unlock(&cpu->internal_mutex);

        log_debug(logger, "Proceso %d desalojado y agregado a la cola de ready", process->pid);
    } else {
        log_warning(logger, "No se encontró el proceso asociado a la CPU %d para agregarlo a la cola de listo para ejecutar después de la confirmación de evict", cpu->id);
    }
    pthread_mutex_unlock(&scheduler_mutex);

    sem_post(&short_term_scheduler_sem); // Signal the short term scheduler that a process was evicted and is ready to be scheduled again

    return NULL;
}

void evict_all_processes (t_interrupt_reason reason) {
    while (1) {
        pthread_mutex_lock(&scheduler_mutex);

        if (list_size(exec_processes) == 0) {
            pthread_mutex_unlock(&scheduler_mutex);
            break;
        }

        t_process *process = list_get(exec_processes, 0);

        t_client_info *cpu = process->cpu;

        remove_process_from_list(exec_processes, process);
        process_set_state(process, READY, logger);
        add_process_to_ready_queue(process);

        pthread_mutex_unlock(&scheduler_mutex);

        evict_process(cpu, reason);
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
