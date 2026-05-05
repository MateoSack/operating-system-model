#include <short_term_scheduler.h>

void short_term_scheduler () { // Main function for the short-term scheduler
    bool no_processes = false;
    bool no_cpus = false;

    while (1) {
        pthread_mutex_lock(&scheduler_mutex);
        
        t_process *process = get_next_process_to_execute();
        t_client_info *cpu = get_available_cpu();

        no_processes = (process == NULL);
        no_cpus = (cpu == NULL);

        if (no_processes || no_cpus) {
            pthread_mutex_unlock(&scheduler_mutex);
            break;
        }

        cpu->is_available = false;
        process_set_state(process, EXEC, logger);
        process_set_cpu(process, cpu);

        pthread_mutex_unlock(&scheduler_mutex);

        send_process_exec_info(process->pid, cpu->fd);
    }

    if (no_processes) log_debug(logger, "No processes in READY");
    if (no_cpus) log_debug(logger, "No CPUs available");
}

t_process *get_next_process_to_execute () { // Returns the next process to execute based on the scheduling algorithm
    t_process *process;

    if (list_is_empty(ready_queue)) return NULL;

    switch (scheduler_algorithm) {
        case FIFO:
            process = list_remove(ready_queue, 0); // Get the first process in the list (FIFO)
            break;
        case RR:
            /* code */
            break;
        case CMN:
            /* code */
            break;
    }

    return process;
}

t_client_info *get_available_cpu () { // Returns an available CPU from the list of CPUs
    t_client_info *cpu;

    bool cpu_is_available(void *ptr) {
        t_client_info *client = (t_client_info*) ptr;
        return client->is_available == true;
    }

    cpu = list_find(list_cpu, (void*)cpu_is_available);

    return cpu;
}

bool process_is_ready(void *ptr) { // Returns true if the process is in READY state, false otherwise
    t_process *process = (t_process*) ptr;
    return process->state == READY;
}

void send_process_exec_info (uint32_t pid, int cpu_fd) { // Sends the process execution information to the CPU
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_EXECUTE;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_send(pkg, cpu_fd);
    package_delete(pkg);
}
