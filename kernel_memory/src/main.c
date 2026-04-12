#include <utils/net_utils.h>
#include <pthread.h>
#include <commons/collections/list.h>

t_log *logger;

int fd_kernel_scheduler = -1;
int fd_io = -1;
int fd_swap = -1;

t_list *list_cpu = NULL;
t_list *list_memory_stick = NULL;

void *handle_module(void *fd_ptr) {
    int fd_client = *(int *)fd_ptr; //trato fd_ptr como puntero a int y obtengo el valor para fd_client
    free(fd_ptr);

    //HANDSHAKE
    int cod_op = operation_receive(fd_client);
    if (cod_op != HANDSHAKE) {
        log_error(logger, "Expected HANDSHAKE, received: %d", cod_op);
        close(fd_client);
        return NULL;
    }

    int size;
    t_module_id *module_id_ptr = buffer_receive(&size, fd_client);
    t_module_id module_id = *module_id_ptr;
    free(module_id_ptr);

    //Handle de nuevo módulo
    switch (module_id) {
        case MODULE_KERNEL_SCHEDULER:
            fd_kernel_scheduler = fd_client;
            log_info(logger, "Kernel Scheduler connected.");
            break;

        case MODULE_CPU: {
            int *fd_cpu_entry = malloc(sizeof(int));
            *fd_cpu_entry = fd_client;
            list_add(list_cpu, fd_cpu_entry);
            log_info(logger, "CPU connected (total: %d)", list_size(list_cpu));
            break;
        }

        case MODULE_IO:
            fd_io = fd_client;
            log_info(logger, "IO connected");
            break;

        case MODULE_SWAP:
            fd_swap = fd_client;
            log_info(logger, "Swap connected");
            break;

        case MODULE_MEMORY_STICK: {
            int *fd_memory_stick_entry = malloc(sizeof(int));
            *fd_memory_stick_entry = fd_client;
            list_add(list_memory_stick, fd_memory_stick_entry);
            log_info(logger, "Memory Stick connected (total: %d)", list_size(list_memory_stick));
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
