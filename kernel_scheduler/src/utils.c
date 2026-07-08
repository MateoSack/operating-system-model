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
    process->start_block_time = 0;
    process->owned_mutexes = list_create();
    sem_init(&process->io_request_sem, 0, 0);
    sem_init(&process->memory_request_sem, 0, 0);

    return process;
}

void destroy_process (void *arg) {
    t_process *process = (t_process *) arg;
    sem_destroy(&process->io_request_sem);
    sem_destroy(&process->memory_request_sem);
    list_destroy(process->owned_mutexes);
    free(process);
}

void add_process_to_list (t_list *list_processes, t_process *process) { // Add a process to the list of processes
    list_add(list_processes, process);
}

void add_process_to_ready_queue (t_process *process) { // Add a process to the ready queue based on its priority and scheduling algorithm. Use under mutex
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

void add_process_at_start_of_ready_queue (t_process *process) { // Add a process to the start ready queue based on its priority and scheduling algorithm
    if (scheduler_algorithm == CMN) {
        int queue_index;

        if (process->effective_priority >= queue_algorithms_count) {
            log_warning(logger, "Proceso %d tiene una prioridad efectiva (%d) mayor a la cantidad de colas de planificación (%d). Agregándolo a la última cola.", process->pid, process->effective_priority, queue_algorithms_count);
            queue_index = queue_algorithms_count - 1; // If the priority is greater than the number of queues, assign it to the last queue
        } else {
            queue_index = process->effective_priority;
        }

        list_add_in_index(ready_queue[queue_index], 0, process);
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
    list_destroy_and_destroy_elements(list, destroy_process);
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
    package_string_add(pkg, path);
	package_send(pkg, kernel_memory_fd, mutex);
    package_delete(pkg);
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

t_process *get_highest_priority_process (t_list *process_list) {
    if (list_is_empty(process_list)) return NULL;
    
    t_process *highest_priority_process = list_get(process_list, 0);

    for (int i = 1; i < list_size(process_list); i++) {
        t_process *current_process = list_get(process_list, i);

        if (current_process->effective_priority < highest_priority_process->effective_priority) highest_priority_process = current_process;
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

bool send_process_evict (t_client_info *cpu, t_interrupt_reason reason) {
    if (cpu == NULL) {
        log_warning(logger, "Se intento desalojar un CPU = NULL");
        return false;
    }

    pthread_mutex_lock(&cpu->internal_mutex);
    if (cpu->is_evicting == true) {
        pthread_mutex_unlock(&cpu->internal_mutex);
        return false;
    }
    cpu->is_evicting = true;
    pthread_mutex_unlock(&cpu->internal_mutex);

    t_package *pkg = package_create();
    pkg->op_code = PROCESS_EVICT;
    package_add(pkg, &reason, sizeof(t_interrupt_reason));
    package_send(pkg, cpu->fd, &cpu->network_mutex);
    package_delete(pkg);

    log_debug(logger, "Enviada orden de evict al CPU %d por motivo de %s", cpu->id, interrupt_reason_to_string(reason));

    return true;
}

bool evict_process(t_client_info *cpu, t_interrupt_reason reason, bool should_handle_state) {
    if (!send_process_evict(cpu, reason)) return false;

    pthread_t thread;
    if (should_handle_state) {
        pthread_create(&thread, NULL, wait_confirmation_thread_and_handle_state, (void*)cpu);
    } else {
        pthread_create(&thread, NULL, wait_confirmation_thread, (void*)cpu);
    }
    
    pthread_detach(thread);

    return true;
}

void *wait_confirmation_thread_and_handle_state (void *arg) { // Wait for a confirmation from the CPU that the process was successfully evicted and is ready to be sent to the ready queue
    t_client_info *cpu = (t_client_info*)arg;

    sem_wait(&cpu->response_sem);

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

        pthread_mutex_unlock(&scheduler_mutex);

        log_debug(logger, "Proceso %d desalojado y agregado a la cola de ready", process->pid);
    } else {
        pthread_mutex_lock(&cpu->internal_mutex);
        cpu->is_available = true;
        cpu->is_evicting = false;
        pthread_mutex_unlock(&cpu->internal_mutex);
        pthread_mutex_unlock(&scheduler_mutex);

        log_warning(logger, "No se encontró el proceso asociado a la CPU %d para agregarlo a la cola de listo para ejecutar después de la confirmación de evict", cpu->id);
    }

    sem_post(&short_term_scheduler_sem); // Signal the short term scheduler that a process was evicted and is ready to be scheduled again

    return NULL;
}

void *wait_confirmation_thread(void *arg) { // Wait for a confirmation from the CPU that the process was successfully evicted. State must be handle by caller
    t_client_info *cpu = (t_client_info*)arg;

    sem_wait(&cpu->response_sem);

    pthread_mutex_lock(&scheduler_mutex);
    t_process *process = get_process_from_cpu(cpu);
    if (process != NULL) {
        remove_process_from_list(exec_processes, process);
        process_set_cpu(process, NULL);
        pthread_mutex_lock(&cpu->internal_mutex);
        cpu->is_available = true;
        cpu->is_evicting = false;
        pthread_mutex_unlock(&cpu->internal_mutex);
        // the caller handles process state
    }
    pthread_mutex_unlock(&scheduler_mutex);
    log_debug(logger, "Proceso desalojado y CPU marcada como libre");
    sem_post(&short_term_scheduler_sem);
    return NULL;
}

void evict_all_processes (t_interrupt_reason reason) {
    pthread_mutex_lock(&scheduler_mutex);
    int count = list_size(exec_processes);
    if (count == 0) {
        pthread_mutex_unlock(&scheduler_mutex);
        return;
    }

    t_client_info **cpus = malloc(count * sizeof(t_client_info*));
    for (int i = 0; i < count; i++) {
        t_process *process = list_get(exec_processes, i);
        cpus[i] = process->cpu;
    }
    pthread_mutex_unlock(&scheduler_mutex);

    t_list *evicted_cpus = list_create();

    for (int i = 0; i < count; i++) {
        if (send_process_evict(cpus[i], reason)) list_add(evicted_cpus, cpus[i]);
    }

    for (int i = 0; i < list_size(evicted_cpus); i++) {
        t_client_info *current_cpu = list_get(evicted_cpus, i);

        sem_wait(&current_cpu->response_sem);
        pthread_mutex_lock(&scheduler_mutex);
        t_process *process = get_process_from_cpu(current_cpu);
        if (process != NULL) {
            remove_process_from_list(exec_processes, process);
            process_set_state(process, READY, logger);
            process_set_cpu(process, NULL);
            add_process_at_start_of_ready_queue(process);
            pthread_mutex_lock(&current_cpu->internal_mutex);
            current_cpu->is_available = true;
            current_cpu->is_evicting = false;
            pthread_mutex_unlock(&current_cpu->internal_mutex);
        }
        pthread_mutex_unlock(&scheduler_mutex);
    }

    free(cpus);
    list_destroy(evicted_cpus);
}

void send_pid_to_execute (uint32_t pid, t_client_info *cpu) { // Sends the process execution information to the CPU
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_EXECUTE;
    package_add(pkg, &pid, sizeof(uint32_t));

    package_send(pkg, cpu->fd, &cpu->network_mutex);

    package_delete(pkg);

    log_debug(logger, "Enviado proceso %d a CPU %d (fd: %d)", pid, cpu->id, cpu->fd);
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

bool can_schedule_get () {
    pthread_mutex_lock(&can_schedule_mutex);
    bool result = can_schedule;
    pthread_mutex_unlock(&can_schedule_mutex);
    return result;
}

void can_schedule_write(bool new_value) {
    pthread_mutex_lock(&scheduler_mutex);
    pthread_mutex_lock(&can_schedule_mutex);
    can_schedule = new_value;
    pthread_mutex_unlock(&can_schedule_mutex);
    pthread_mutex_unlock(&scheduler_mutex);
}

void process_set_priority (t_process *process, uint8_t new_priority) { // Use under mutex
    uint8_t old_priority = process->effective_priority;

    process->effective_priority = new_priority;

    log_info(logger, "## <%d> Cambio de prioridad: <%d> - <%d>", process->pid, old_priority, new_priority);
}

void send_memory_write (uint32_t pid, uint32_t physical_address, uint32_t size, char *data) {
    t_package *pkg = package_create();
    pkg->op_code = IO_MEMORY_WRITE;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &physical_address, sizeof(uint32_t));
    package_add(pkg, &size, sizeof(uint32_t));
    package_string_add(pkg, data);
	package_send(pkg, kernel_memory->fd, &kernel_memory->network_mutex);
    package_delete(pkg);
}

void send_memory_read (uint32_t pid, uint32_t physical_address, uint32_t size) {
    t_package *pkg = package_create();
    pkg->op_code = IO_MEMORY_READ;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &physical_address, sizeof(uint32_t));
    package_add(pkg, &size, sizeof(uint32_t));
	package_send(pkg, kernel_memory->fd, &kernel_memory->network_mutex);
    package_delete(pkg);
}

t_list *sort_processes_by_priority (t_list *process_list) { // Sorts a list of processes by their effective priority, returns a new sorted list
    t_list *sorted_list = list_duplicate(process_list);

    bool _compare_process_priority(void *a, void *b) {
        t_process *process_a = (t_process*)a;
        t_process *process_b = (t_process*)b;
        return process_a->effective_priority < process_b->effective_priority; // 0 is the highest priority
    }

    list_sort(sorted_list, _compare_process_priority);

    return sorted_list;
}

t_list *process_list_to_pid_list(t_list *process_list) { // Converts a list of processes to a list of their PIDs, returns a new list of PIDs
    t_list *pid_list = list_create();

    for (int i = 0; i < list_size(process_list); i++) {
        t_process *process = list_get(process_list, i);
        uint32_t *pid = malloc(sizeof(uint32_t));
        *pid = process->pid;
        list_add(pid_list, pid);
    }

    return pid_list;
}

void process_set_ready (t_process *process) { // Set process state to READY or SUSP_READY based on its current state. Use under mutex
    if (process->state == SUSP_BLOCK) process_set_state(process, SUSP_READY, logger);
    else process_set_state(process, READY, logger);
}
