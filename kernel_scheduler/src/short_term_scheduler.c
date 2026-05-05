#include <short_term_scheduler.h>

int short_term_scheduler () {
    t_process *process = get_next_process_to_execute(list_processes);
    t_client_info *cpu = get_available_cpu();

    if (process == NULL) {
        log_info(logger, "No processes in READY state");
        return EXIT_SUCCESS;
    }

    if (cpu == NULL) {
        log_info(logger, "No CPU available");
        return EXIT_FAILURE;
    }

    process_set_state(process, EXEC, logger);
    send_process_exec_info(process->pid, cpu->fd);
    return EXIT_SUCCESS;
}

t_process *get_next_process_to_execute (t_list *list_processes) {
    t_process *process;

    switch (scheduler_algorithm) {
        case FIFO:
            process = list_find(list_processes, (void*)process_is_ready);
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

t_client_info *get_available_cpu () {
    t_client_info *cpu;

    bool cpu_is_available(t_client_info *p) {
        return p->is_available == true;
    }

    cpu = list_find(list_cpu, (void*)cpu_is_available);

    return cpu;
}

bool process_is_ready(t_process *p) {
    return p->state == READY;
}

void send_process_exec_info (uint32_t pid, int cpu_fd) {
    t_package *pkg = package_create();
    pkg->op_code = PROCESS_EXECUTE;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_send(pkg, cpu_fd);
    package_delete(pkg);
}
