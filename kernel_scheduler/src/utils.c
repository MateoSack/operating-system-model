#include <utils.h>

void process_set_state (t_process *process, t_process_state state, t_log *logger) {
    log_info(logger, "## (%d) Transitions from state <%s> to state <%s>", process->pid, process_state_to_string(process->state), process_state_to_string(state));
    process->state = state;
}

t_process *add_process_to_list (t_list *list_processes, uint32_t pid, uint8_t priority) {
    t_process *process = malloc(sizeof(t_process));
    process->pid = pid;
    process->priority = priority;
    process->state = READY;
    process->cpu_id = -1;

    list_add(list_processes, process);
    return process;
}

void send_process_create_info (uint32_t pid, char *path, int kernel_memory_fd) {
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_CREATE;
    package_add(pkg, &pid, sizeof(uint32_t));
	package_send(pkg, kernel_memory_fd);
    package_delete(pkg);

    message_send(path, kernel_memory_fd);
}

t_scheduler_algorithm scheduler_algorithm_from_string(const char *str) {
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
