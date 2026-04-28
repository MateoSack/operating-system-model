#include <utils.h>

void process_set_state (t_process *process, t_process_state state, t_log *logger) {
    log_info(logger, "## (%d) Pasa del estado <%s> al estado <%s>", process->pid, process_state_to_string(process->state), process_state_to_string(state));
    process->state = state;
}

void add_process_to_list (t_list *list_processes, uint32_t pid, uint8_t priority) {
    t_process *process = malloc(sizeof(t_process));
    process->pid = pid;
    process->priority = priority;
    process->state = READY;

    list_add(list_processes, process);
}
