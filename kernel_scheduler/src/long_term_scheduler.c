#include <long_term_scheduler.h>

int long_term_scheduler (t_log *logger, t_list *list_processes, uint32_t *current_pid, char *path, uint8_t priority, int kernel_memory_fd) {
    uint32_t pid = pid_assigner(current_pid);
    t_pcb *pcb = malloc(sizeof(t_pcb));
    create_pcb(pcb, pid, priority);

    log_info(logger, "## (%d) Se crea el proceso - Estado: NEW", pid);

    pcb_set_state(pcb, READY, logger);
    pcb_send(pcb, kernel_memory_fd);
    message_send(path, kernel_memory_fd);
    free(pcb);

    add_process_to_list(list_processes, pid, priority);

    return EXIT_SUCCESS;
}

uint32_t pid_assigner (uint32_t *current_pid) {
    uint32_t assigned_pid = *current_pid;
    (*current_pid)++;
    return assigned_pid;
}
