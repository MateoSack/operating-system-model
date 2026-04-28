#include <main.h>

t_log *logger;
t_config *config = NULL;

int kernel_scheduler_fd = -1;
int swap_fd = -1;

t_list *list_cpu = NULL;
t_list *list_memory_stick = NULL;
t_list *list_memory_stick_credentials = NULL;
t_list *list_processes = NULL;

uint32_t next_memory_stick_id = 0;

int main(void) {
    /*-------------------Initial Setup-------------------*/
    config = config_create("kernel_memory.config");
    if(config == NULL) return EXIT_FAILURE;
    logger = start_logger(config);

    list_cpu          = list_create();
    list_memory_stick = list_create();
    list_memory_stick_credentials = list_create();
    list_processes = list_create();

	char *port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

    int server_fd = server_start(port, logger);

    if (server_fd == -1) {
        log_error(logger, "Couldn't start server.");
        return EXIT_FAILURE;
    }

    log_info(logger, "Kernel Memory ready, now waiting...");

/*-------------------Handle connections-------------------*/
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

    log_destroy(logger);
    config_destroy(config);
    // Liberar listas y demás cosas
    return EXIT_SUCCESS;
}

void *handle_module(void *fd_ptr) {
    int client_fd = *(int *)fd_ptr; //trato fd_ptr como puntero a int y obtengo el valor para client_fd
    free(fd_ptr);

    t_module_id module_id = handshake_receiver(client_fd);

    /*-------------------Handle new module-------------------*/
    switch (module_id) {
        case MODULE_KERNEL_SCHEDULER: {
            kernel_scheduler_fd = client_fd;
            log_info(logger, "Kernel Scheduler connected.");
            if(kernel_scheduler_handler(logger, client_fd, config) == -1) return NULL;
        }

        case MODULE_CPU: {
            // CPU ya viene con su ID asignado por Scheduler
            int cpu_id = uint32_receive(client_fd);
            t_client_info *cpu = malloc(sizeof(t_client_info));
	        cpu = add_client_to_list(list_cpu, client_fd, cpu_id);
            send_credentials_list(client_fd, list_memory_stick_credentials, logger);

            log_info(logger, "CPU %d conectada (total: %d)", cpu->id, list_size(list_cpu));

            if(cpu_handler(logger, client_fd, cpu_id) == -1) return NULL; // IMPLEMENTAR: Cierre verdadero
        }
        
        case MODULE_SWAP: {
            swap_fd = client_fd;
            log_info(logger, "Swap connected");
            if(swap_handler(logger, swap_fd) == -1) return NULL; // IMPLEMENTAR: Cierre verdadero (tal vez falta el free client_fd)
        }

        case MODULE_MEMORY_STICK: {
            t_module_credentials *ms_credentials = memory_stick_protocol(logger, client_fd);
            if(memory_stick_handler(logger, client_fd, ms_credentials) == -1) return NULL; // IMPLEMENTAR: Cierre verdadero, y log de BSOD
        }

        default: {
            log_warning(logger, "Unknown module: %d", module_id);
            close(client_fd);
            return NULL;
        }
    }
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level log_level = log_level_from_string(level_str);
	t_log *logger = log_create("kernel_memory.log", "KernelMemory", 1, log_level);
	return logger;
}

void update_cpu_list(t_module_credentials *new_cred) {
    log_debug(logger, "Updating cpu list of credentials");
    for (int i = 0; i < list_size(list_cpu); i++) {
        t_client_info *cpu = list_get(list_cpu, i);
        log_debug(logger, "Retreived from list fd: %d, id: %d", cpu->fd, cpu->id);
        send_credentials(cpu->fd, new_cred, logger);
    }
}

t_module_credentials *memory_stick_protocol (t_log *logger, int client_fd){
    int ms_id = next_memory_stick_id;
    uint32_send(client_fd, ms_id);
    next_memory_stick_id++;

	t_client_info *memory_stick = malloc(sizeof(t_client_info));
	memory_stick = add_client_to_list(list_memory_stick, client_fd, ms_id);
    
    log_info(logger, "Memory Stick %d conectado (total: %d)", memory_stick->id, list_size(list_memory_stick));
    t_module_credentials *client = malloc(sizeof(t_module_credentials));
    client->ip = message_receive(logger, client_fd);
    client->port = message_receive(logger, client_fd);
    client->id = memory_stick->id;
    list_add(list_memory_stick_credentials, client);

    if(list_size(list_cpu) != 0) update_cpu_list(client);

    return client;
}
