#include <main.h>

t_log *logger;

int fd_kernel_scheduler = -1;
int swap_fd = -1;

t_list *list_cpu = NULL;
t_list *list_memory_stick = NULL;

int next_memory_stick_id = 0;

int main(void) {
    t_config *config = config_create("kernel_memory.config");
    if(config == NULL) return EXIT_FAILURE;
    logger = start_logger(config);

    list_cpu          = list_create();
    list_memory_stick = list_create();

    int server_fd = server_start(logger);
    if (server_fd == -1) {
        log_error(logger, "Couldn't start server.");
        return EXIT_FAILURE;
    }

    log_info(logger, "Kernel Memory ready, now waiting...");

    while (1) {
        int new_client_fd = server_client_wait(server_fd);
        if (new_client_fd == -1) {
            log_error(logger, "Failed to accept connection.");
            continue;
        }

        log_debug(logger, "New client connected: %d", new_client_fd);

        int *thread_fd = malloc(sizeof(int));
        *thread_fd = new_client_fd;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_module, thread_fd);
        pthread_detach(thread);
    }

    return EXIT_SUCCESS;
}

void *handle_module(void *fd_ptr) {
    int client_fd = *(int *)fd_ptr; //trato fd_ptr como puntero a int y obtengo el valor para client_fd
    free(fd_ptr);

    t_module_id module_id = handshake_receiver(client_fd);

    //Handle de nuevo módulo
    switch (module_id) {
        case MODULE_KERNEL_SCHEDULER:
            fd_kernel_scheduler = client_fd;
            log_info(logger, "Kernel Scheduler connected.");
            break;

        case MODULE_CPU: {
        // CPU ya viene con su ID asignado por Scheduler
        // IMPORTANTE: Confirmar si funciona como bloqueante el buffer_receive y efectivamente espera el mensaje de ID del CPU. Sino implementar que si lo espere.
        
        int cpu_id = id_receive(client_fd);
        add_client_to_list(list_cpu, client_fd, cpu_id);

        log_info(logger, "CPU %d conectada (total: %d)", cpu_id, list_size(list_cpu));
        break;
    }
        
        case MODULE_SWAP:
            swap_fd = client_fd;
            log_info(logger, "Swap connected");
            break;

        case MODULE_MEMORY_STICK: {
            int ms_id = id_assigner(&next_memory_stick_id, client_fd);
            add_client_to_list(list_memory_stick, client_fd, ms_id);

            log_info(logger, "Memory Stick %d conectado (total: %d)", ms_id, list_size(list_memory_stick));
            break;
    }

        default:
            log_warning(logger, "Unknown module: %d", module_id);
            close(client_fd);
            return NULL;
    }

    // --- LOOP DE ATENCIÓN ---
    while (1) {
        int op = operation_receive(client_fd);
        if (op == -1) {
            log_warning(logger, "Module %d disconnected", module_id);
            break;
        }
        // TODO: manejar operaciones de cada módulo ///////////// ver de hacer todo esto en otro archivo en vez de main
    }

    close(client_fd);
    return NULL;
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("kernel_memory.log", "KernelMemory", 1, level);
	return logger;
}
