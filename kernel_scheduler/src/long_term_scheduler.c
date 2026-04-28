#include <long_term_scheduler.h>

int long_term_scheduler (char *path, uint8_t priority) {
    uint32_t pid = pid_assigner(&current_max_pid);
    t_process *process;

    log_info(logger, "## (%d) New process - State: NEW", pid);
    send_process_create_info(pid, path, kernel_memory_fd);
    free(path);
    
    process = add_process_to_list(list_processes, pid, priority);
    process_set_state(process, READY, logger); //Currently not doing anything

    return EXIT_SUCCESS;
}

uint32_t pid_assigner (uint32_t *current_pid) {
    uint32_t assigned_pid = *current_pid;
    (*current_pid)++;
    return assigned_pid;
}
