#include <utils/server_utils.h>
#include <commons/collections/list.h>

t_log *logger;

int fd_kernel_scheduler = -1;
int fd_io = -1;
int fd_swap = -1;

t_list *list_cpu = NULL;
t_list *list_memory_stick = NULL;

int next_memory_stick_id = 0;

void *handle_module(void *fd_ptr) {
    int fd_client = *(int *)fd_ptr; //trato fd_ptr como puntero a int y obtengo el valor para fd_client
    free(fd_ptr);

    t_module_id module_id = handshake_receiver(fd_client);

    //Handle de nuevo módulo
    switch (module_id) {
        case MODULE_KERNEL_SCHEDULER:
            fd_kernel_scheduler = fd_client;
            log_info(logger, "Kernel Scheduler connected.");
            break;

        case MODULE_CPU: {
        // CPU ya viene con su ID asignado por Scheduler
        // IMPORTANTE: Confirmar si funciona como bloqueante el buffer_receive y efectivamente espera el mensaje de ID del CPU. Sino implementar que si lo espere.
        
        int cpu_id = id_receive(fd_client);

        t_client_info *cpu = malloc(sizeof(t_client_info));
        cpu->fd = fd_client;
        cpu->id = *cpu_id_ptr;
        free(cpu_id_ptr);

        list_add(list_cpu, cpu);
        log_info(logger, "CPU %d conectada (total: %d)", cpu->id, list_size(list_cpu));
        break;
    }
        
        case MODULE_SWAP:
            fd_swap = fd_client;
            log_info(logger, "Swap connected");
            break;

        case MODULE_MEMORY_STICK: {
            int ms_id = id_assigner(&next_memory_stick_id, fd_client);
            add_client_to_list(list_memory_stick, fd_client, ms_id);

            t_package *pkg = package_create();
            package_add(pkg, &ms->id, sizeof(int));
            package_send(pkg, fd_client);
            package_delete(pkg);

            log_info(logger, "Memory Stick %d conectado (total: %d)", ms->id, list_size(list_memory_stick));
            break;
    }

        default:
            log_warning(logger, "Unknown module: %d", module_id);
            close(fd_client);
            return NULL;
    }

    // --- LOOP DE ATENCIÓN ---
    while (1) {
        int op = operation_receive(fd_client);
        if (op == -1) {
            log_warning(logger, "Module %d disconnected", module_id);
            break;
        }
        // TODO: manejar operaciones de cada módulo ///////////// ver de hacer todo esto en otro archivo en vez de main
    }

    close(fd_client);
    return NULL;
}

int main(void) {
    logger = log_create("kernel_memory.log", "KernelMemory", 1, LOG_LEVEL_DEBUG);

    list_cpu          = list_create();
    list_memory_stick = list_create();

    int server_fd = server_start(logger);
    if (server_fd == -1) {
        log_error(logger, "Couldn't start server.");
        return EXIT_FAILURE;
    }

    log_info(logger, "Kernel Memory ready, now waiting...");

    while (1) {
        int fd_new_client = server_client_wait(server_fd);
        if (fd_new_client == -1) {
            log_error(logger, "Failed to accept connection.");
            continue;
        }

        int *fd_thread = malloc(sizeof(int));
        *fd_thread = fd_new_client;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_module, fd_thread);
        pthread_detach(thread);
    }

    return EXIT_SUCCESS;
}
