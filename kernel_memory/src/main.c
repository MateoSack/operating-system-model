#include <main.h>
#include <string.h>

char *allocation_strategy = NULL;

t_log *logger;
t_config *config = NULL;

t_client_info *kernel_scheduler = NULL;
int swap_fd = -1;
pthread_mutex_t swap_network_mutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t total_memory_size = 0;
uint32_t segment_max_size = 0;
uint32_t swap_block_size = 0;
uint32_t swap_total_size = 0;
uint32_t instruction_delay = 0;
uint32_t compaction_delay = 0;

pthread_mutex_t total_memory_size_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_cpu_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_memory_stick_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_memory_stick_credentials_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_processes_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t next_memory_stick_id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_suspended_processes_mutex = PTHREAD_MUTEX_INITIALIZER;

t_swap_read_response swap_read_response = {
    .data  = NULL,
    .size  = 0,
    .ready = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
};

sem_t sem_swap_write_done;
sem_t compaction_sem;
sem_t swap_read_response_sem;

t_list *list_cpu = NULL;
t_list *list_memory_stick = NULL;
t_list *list_memory_stick_credentials = NULL;
t_list *list_processes = NULL;
t_list *list_suspended_processes = NULL;

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

    char *strategy_cfg = config_get_string_value(config, "ALLOCATION_STRATEGY");
    if (strategy_cfg == NULL) {
        log_error(logger, "ALLOCATION_STRATEGY not set in config, exiting");
        log_destroy(logger);
        config_destroy(config);
        return EXIT_FAILURE;
    }
    allocation_strategy = strdup(strategy_cfg);

    instruction_delay = (uint32_t)config_get_int_value(config, "INSTRUCTION_DELAY"); // In milliseconds
    compaction_delay = (uint32_t)config_get_int_value(config, "COMPACTION_DELAY"); // In milliseconds

    segment_max_size = (uint32_t)config_get_int_value(config, "SEGMENT_MAX_SIZE"); // In bytes
    if (segment_max_size <= 0) {
        log_error(logger, "SEGMENT_MAX_SIZE invalid or not set in config, exiting");
        log_destroy(logger);
        config_destroy(config);
        free(allocation_strategy);
        return EXIT_FAILURE;
    }
    log_debug(logger, "SEGMENT_MAX_SIZE read from config: %u", segment_max_size);

    list_cpu = list_create();
    list_memory_stick = list_create();
    list_memory_stick_credentials = list_create();
    list_processes = list_create();
    list_suspended_processes = list_create();

    sem_init(&compaction_sem, 0, 0);
    sem_init(&sem_swap_write_done, 0, 0);
    sem_init(&swap_read_response_sem, 0, 0);

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
    free(allocation_strategy);
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
            uint32_t current_total = 0;

            kernel_scheduler = create_client_info(client_fd, 0); // ID no necesario para Kernel Scheduler, se puede setear en 0

            pthread_mutex_lock(&total_memory_size_mutex);
            current_total = total_memory_size;
            pthread_mutex_unlock(&total_memory_size_mutex);

            if (current_total > 0) send_memory_update(current_total);

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
            send_credentials_list(cpu->fd, list_memory_stick_credentials, logger, &cpu->network_mutex);
            pthread_mutex_unlock(&list_memory_stick_credentials_mutex);

            log_debug(logger, "Credentials list sent to CPU");
            
            log_debug(logger, "Sending SEGMENT_MAX_SIZE (%u) to CPU %d", segment_max_size, cpu->id);
            uint32_send(cpu->fd, segment_max_size, &cpu->network_mutex);

            log_info(logger, "## CPU %d Conectada", cpu->id);
            log_info(logger, "Total de CPUs conectadas: %d", cpu_count);

            if(cpu_handler(logger, cpu) == -1) return NULL; // IMPLEMENTAR: Cierre verdadero
            break;
        }

        case MODULE_SWAP: {
            swap_fd = client_fd;
            log_info(logger, "Swap connected");
            if(swap_handler(logger, swap_fd) == -1) return NULL; // IMPLEMENTAR: Cierre verdadero (tal vez falta el free client_fd)
            break;
        }

        case MODULE_MEMORY_STICK: {
            t_memory_stick_credentials *ms_credentials = memory_stick_protocol(logger, client_fd);
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

void update_cpu_list(t_memory_stick_credentials *new_cred) {
    log_debug(logger, "Updating cpu list of credentials");

    pthread_mutex_lock(&list_cpu_mutex);
    t_list *cpu_list_copy = list_duplicate(list_cpu);
    pthread_mutex_unlock(&list_cpu_mutex);

    int cpu_count = list_size(cpu_list_copy);
    for (int i = 0; i < cpu_count; i++) {
        t_client_info *cpu = list_get(cpu_list_copy, i);
        log_debug(logger, "Retreived from list fd: %d, id: %d", cpu->fd, cpu->id);
        send_credentials(cpu->fd, new_cred, logger, &cpu->network_mutex);
    }

    list_destroy(cpu_list_copy);
}

t_memory_stick_credentials *memory_stick_protocol (t_log *logger, int client_fd){
    pthread_mutex_lock(&next_memory_stick_id_mutex);
    int ms_id = next_memory_stick_id;
    next_memory_stick_id++;
    pthread_mutex_unlock(&next_memory_stick_id_mutex);

	t_memory_stick_info *memory_stick = malloc(sizeof(t_memory_stick_info));
    memory_stick->fd = client_fd;
    memory_stick->id = ms_id;
    pthread_mutex_init(&memory_stick->mutex, NULL);
    pthread_mutex_init(&memory_stick->network_mutex, NULL);
    sem_init(&memory_stick->response_sem, 0, 0);
    
    uint32_send(client_fd, ms_id, &memory_stick->network_mutex);
    uint32_t ms_size = uint32_receive(client_fd);
    memory_stick->size = ms_size;
    
    pthread_mutex_lock(&total_memory_size_mutex);
    total_memory_size += ms_size;
    uint32_t current_total = total_memory_size;
    pthread_mutex_unlock(&total_memory_size_mutex);

    if (kernel_scheduler != NULL && kernel_scheduler->fd != -1) {
        send_memory_update(current_total);
        log_debug(logger, "Aviso de actualizacion de memoria enviado a Kernel Scheduler de %d bytes", current_total);
    }

    pthread_mutex_lock(&list_memory_stick_mutex);
    list_add(list_memory_stick, memory_stick);
    int ms_count = list_size(list_memory_stick); // Total number of memory sticks connected
    pthread_mutex_unlock(&list_memory_stick_mutex);
    
    log_info(logger, "## Memory Stick de %d bytes Conectada", ms_size);
    log_debug(logger, "Total de Memory Sticks conectadas: %d", ms_count);

    t_memory_stick_credentials *client = malloc(sizeof(t_memory_stick_credentials));
    client->ip = message_receive(logger, client_fd);
    client->port = message_receive(logger, client_fd);
    client->id = memory_stick->id;
    client->size = ms_size;

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
