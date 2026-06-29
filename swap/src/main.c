#include <main.h>

t_log *logger;
t_client_info *kernel_memory = NULL;

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Modo de uso: %s <archivo_config>\n", argv[0]);
        return EXIT_FAILURE;
    }

    t_config *config = config_create(argv[1]);
    if(config == NULL) return EXIT_FAILURE;

	logger = start_logger(config);

	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");
    char *swap_file_path = config_get_string_value(config, "SWAP_FILE_PATH");
    uint32_t swap_file_size = (uint32_t) config_get_int_value(config, "SWAP_FILE_SIZE");
    uint32_t block_size = (uint32_t) config_get_int_value(config, "BLOCK_SIZE");

    log_info(logger, "SWAP iniciado");

    FILE *swap_file = fopen(swap_file_path, "r+b");
    if (swap_file == NULL) {
        // El archivo no existe, lo creamos del tamaño correcto
        swap_file = fopen(swap_file_path, "w+b");
        if (swap_file == NULL) {
            log_error(logger, "No se pudo crear el archivo de SWAP en %s", swap_file_path);
            return EXIT_FAILURE;
        }
        fseek(swap_file, swap_file_size - 1, SEEK_SET);
        fputc('\0', swap_file); // Force the file to be of the correct size
        fflush(swap_file); // Ensure the file is written to disk
    }

    pthread_mutex_t write_mutex = PTHREAD_MUTEX_INITIALIZER;

    int kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

    kernel_memory = create_client_info(kernel_memory_fd, 0);

    if(kernel_memory_fd == -1)
    {
        log_error(logger, "Conexion con Kernel Memory fallida");
        return EXIT_FAILURE;
    }

    t_module_id_send(kernel_memory->fd, MODULE_SWAP, logger, &kernel_memory->network_mutex);
    log_info(logger, "## Conectado a Kernel Memory");
    send_swap_info(kernel_memory, block_size, swap_file_size);

    while (1) {
        int op = operation_receive(kernel_memory->fd);
        if (op == -1) {
            log_warning(logger, "Kernel Memory desconectado");
            destroy_client(kernel_memory);
            break;
        }

        switch (op) {
            case SWAP_IN:
                handle_block_read(kernel_memory->fd, swap_file, block_size, &write_mutex);
                break;

            case SWAP_OUT:
                handle_block_write(kernel_memory->fd, swap_file, block_size, &write_mutex);
                break;

            default:
                log_warning(logger, "Operacion desconocida recibida: %d", op);
                break;
        }
    }

    fclose(swap_file);
    log_destroy(logger);
    config_destroy(config);
    free(kernel_memory_ip);
    free(kernel_memory_port);
    return EXIT_SUCCESS;
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("swap.log", "SWAP", true, level);
	return logger;
}

void send_swap_info(t_client_info *kernel_memory, uint32_t block_size, uint32_t swap_file_size) {
    uint32_send(kernel_memory->fd, block_size, &kernel_memory->network_mutex); // VER SI ESTOY USANDO BIEN EL MUTEX
    uint32_send(kernel_memory->fd, swap_file_size, &kernel_memory->network_mutex);
}

void handle_block_read(int client_fd, FILE *swap_file, uint32_t block_size, pthread_mutex_t *write_mutex) {
    uint32_t block_number = uint32_decode(client_fd);

    void *buffer = malloc(block_size);

    pthread_mutex_lock(write_mutex);
    fseek(swap_file, (long)block_number * block_size, SEEK_SET);
    fread(buffer, block_size, 1, swap_file);
    pthread_mutex_unlock(write_mutex);

    log_info(logger, "## Lectura del bloque: %u", block_number);

    t_package *pkg = package_create();
    pkg->op_code = SWAP_IN;
    package_add(pkg, buffer, block_size);
    package_send(pkg, client_fd, &kernel_memory->network_mutex);
    package_delete(pkg);

    free(buffer);
}

void handle_block_write(int client_fd, FILE *swap_file, uint32_t block_size, pthread_mutex_t *write_mutex) {
    int size;
    int offset = 0;
    void *buffer = buffer_receive(&size, client_fd);

    uint32_t block_number = uint32_deserialize(buffer, &offset);
    void *block_data = buffer + offset;

    pthread_mutex_lock(write_mutex);
    fseek(swap_file, (long)block_number * block_size, SEEK_SET);
    fwrite(block_data, block_size, 1, swap_file);
    fflush(swap_file);
    pthread_mutex_unlock(write_mutex);

    log_info(logger, "## Escritura del bloque: %u", block_number);

    free(buffer);

    t_package *pkg = package_create();
    pkg->op_code = SWAP_OUT;
    package_send(pkg, client_fd, &kernel_memory->network_mutex);
    package_delete(pkg);
}
