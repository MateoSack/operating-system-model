#include <main.h>

t_log *logger;
t_config *config = NULL;

int kernel_scheduler_fd = -1;
int swap_fd = -1;

uint32_t total_memory_size = 0;

pthread_mutex_t total_memory_size_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t kernel_scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_cpu_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_memory_stick_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_memory_stick_credentials_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_processes_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t next_memory_stick_id_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t compaction_sem;

t_list *list_cpu = NULL;
t_list *list_memory_stick = NULL;
t_list *list_memory_stick_credentials = NULL;
t_list *list_processes = NULL;

uint32_t next_memory_stick_id = 0;

int main(int argc, char *argv[]) {
    /*-------------------Initial Setup-------------------*/
    if (argc < 2) {
        printf("Mode of use: %s <config_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    config = config_create(argv[1]);
    if(config == NULL) return EXIT_FAILURE;
    logger = start_logger(config);

    list_cpu = list_create();
    list_memory_stick = list_create();
    list_memory_stick_credentials = list_create();
    list_processes = list_create();

    sem_init(&compaction_sem, 0, 0);

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
            pthread_mutex_lock(&kernel_scheduler_mutex);
            kernel_scheduler_fd = client_fd;
            uint32_t current_total = 0;
            pthread_mutex_lock(&total_memory_size_mutex);
            current_total = total_memory_size;
            pthread_mutex_unlock(&total_memory_size_mutex);
            if (current_total > 0) send_memory_update(kernel_scheduler_fd, current_total);
            pthread_mutex_unlock(&kernel_scheduler_mutex);

            log_info(logger, "## Kernel Scheduler Conectado - FD del socket: %d", client_fd);
            
            if (current_total > 0) log_debug(logger, "Sent initial memory update to Kernel Scheduler: %d bytes", current_total);

            if(kernel_scheduler_handler(logger, client_fd, config) == -1) return NULL;
            break;
        }

        case MODULE_CPU: {
            // CPU ya viene con su ID asignado por Scheduler
            int cpu_id = uint32_receive(client_fd);
            t_client_info *cpu = NULL;

            pthread_mutex_lock(&list_cpu_mutex);
            cpu = add_client_to_list(list_cpu, client_fd, cpu_id);
            int cpu_count = list_size(list_cpu);
            pthread_mutex_unlock(&list_cpu_mutex);

            log_debug(logger, "Trying to send credentials list to CPU");
            pthread_mutex_lock(&list_memory_stick_credentials_mutex);
            send_credentials_list(client_fd, list_memory_stick_credentials, logger);
            pthread_mutex_unlock(&list_memory_stick_credentials_mutex);

            log_debug(logger, "Credentials list sent to CPU");

            log_info(logger, "## CPU %d Conectada", cpu->id);
            log_info(logger, "Total de CPUs conectadas: %d", cpu_count);

            if(cpu_handler(logger, client_fd, cpu_id) == -1) return NULL; // IMPLEMENTAR: Cierre verdadero
            break;
        }

        case MODULE_SWAP: {
            swap_fd = client_fd;
            log_info(logger, "Swap connected");
            if(swap_handler(logger, swap_fd) == -1) return NULL; // IMPLEMENTAR: Cierre verdadero (tal vez falta el free client_fd)
            break;
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

    return NULL;
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level log_level = log_level_from_string(level_str);
	t_log *logger = log_create("kernel_memory.log", "KernelMemory", 1, log_level);
	return logger;
}

void update_cpu_list(t_module_credentials *new_cred) {
    log_debug(logger, "Updating cpu list of credentials");

    pthread_mutex_lock(&list_cpu_mutex);
    t_list *cpu_list_copy = list_duplicate(list_cpu);
    pthread_mutex_unlock(&list_cpu_mutex);

    int cpu_count = list_size(cpu_list_copy);
    for (int i = 0; i < cpu_count; i++) {
        t_client_info *cpu = list_get(cpu_list_copy, i);
        log_debug(logger, "Retreived from list fd: %d, id: %d", cpu->fd, cpu->id);
        send_credentials(cpu->fd, new_cred, logger);
    }

    list_destroy(cpu_list_copy);
}

t_module_credentials *memory_stick_protocol (t_log *logger, int client_fd){
    pthread_mutex_lock(&next_memory_stick_id_mutex);
    int ms_id = next_memory_stick_id;
    next_memory_stick_id++;
    pthread_mutex_unlock(&next_memory_stick_id_mutex);

    uint32_send(client_fd, ms_id);
    uint32_t ms_size = uint32_receive(client_fd);

    pthread_mutex_lock(&total_memory_size_mutex);
    total_memory_size += ms_size;
    uint32_t current_total = total_memory_size;
    pthread_mutex_unlock(&total_memory_size_mutex);

    pthread_mutex_lock(&kernel_scheduler_mutex);
    if (kernel_scheduler_fd != -1) send_memory_update(kernel_scheduler_fd, current_total);
    pthread_mutex_unlock(&kernel_scheduler_mutex);
    if (kernel_scheduler_fd != -1) log_debug(logger, "Sent memory update to Kernel Scheduler: %d bytes", current_total);

	t_memory_stick_info *memory_stick = malloc(sizeof(t_memory_stick_info));
    memory_stick->fd = client_fd;
    memory_stick->id = ms_id;
    memory_stick->size = ms_size;

    pthread_mutex_lock(&list_memory_stick_mutex);
    list_add(list_memory_stick, memory_stick);
    int ms_count = list_size(list_memory_stick); // Total number of memory sticks connected
    pthread_mutex_unlock(&list_memory_stick_mutex);
    
    log_info(logger, "## Memory Stick de %d bytes Conectada", ms_size);
    log_debug(logger, "Total de Memory Sticks conectadas: %d", ms_count);

    t_module_credentials *client = malloc(sizeof(t_module_credentials));
    client->ip = message_receive(logger, client_fd);
    client->port = message_receive(logger, client_fd);
    client->id = memory_stick->id;

    pthread_mutex_lock(&list_memory_stick_credentials_mutex);
    list_add(list_memory_stick_credentials, client);
    pthread_mutex_unlock(&list_memory_stick_credentials_mutex);

    int cpu_count = 0;
    pthread_mutex_lock(&list_cpu_mutex);
    cpu_count = list_size(list_cpu);
    pthread_mutex_unlock(&list_cpu_mutex);

    if(cpu_count != 0) update_cpu_list(client);

    return client;
}
