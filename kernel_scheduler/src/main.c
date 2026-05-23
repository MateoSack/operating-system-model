#include <main.h>

t_log *logger;
t_config *config;

t_scheduler_algorithm scheduler_algorithm;
int quantum = 0;

t_list *list_cpu = NULL;
t_list *list_io_sleep = NULL;
t_list *list_io_stdin = NULL;
t_list *list_io_stdout = NULL;

t_list *list_processes = NULL;
t_list *ready_queue = NULL;
t_list *exec_processes = NULL;

t_list *list_mutexes = NULL;

t_temporal *system_timer;

pthread_mutex_t scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_mutex_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t io_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cpu_id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t io_id_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t short_term_scheduler_sem;

sem_t shutdown_sem;

int kernel_memory_fd = -1;

uint32_t next_cpu_id = 0;
uint32_t next_io_id = 0;

uint32_t current_max_pid = 0;

int main(int argc, char *argv[]) {
	/*-------------------Initial Setup-------------------*/
	if (argc < 3) {
        printf("Modo de uso: %s <config_file> <first_process_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

	config = config_create(argv[1]);
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);

	scheduler_algorithm = scheduler_algorithm_from_string(config_get_string_value(config, "PLANIFICATION_ALGORITHM"));
	quantum = config_get_int_value(config, "RR_QUANTUM");

	list_cpu = list_create();
    list_io_sleep = list_create();
	list_io_stdin = list_create();
	list_io_stdout = list_create();
	list_processes = list_create();
	ready_queue = list_create();
	exec_processes = list_create();
	list_mutexes = list_create();

	sem_init(&short_term_scheduler_sem, 0, 0);
	sem_init(&shutdown_sem, 0, 0);

	pthread_t shutdown_thread;
	pthread_create(&shutdown_thread, NULL, shutdown_handler, NULL);
	pthread_detach(shutdown_thread);

	pthread_t short_term_scheduler_thread;
	pthread_create(&short_term_scheduler_thread, NULL, short_term_scheduler_main, NULL);
	pthread_detach(short_term_scheduler_thread);

	if (scheduler_algorithm == RR) {
		system_timer = temporal_create();

        pthread_t quantum_thread;
        pthread_create(&quantum_thread, NULL, quantum_manager, NULL);
        pthread_detach(quantum_thread);
	}

	/*-------------------Connection with Kernel Memory-------------------*/
	if(kernel_memory_connection(logger, config, strdup(argv[2])) == EXIT_FAILURE) return EXIT_FAILURE;

	/*-------------------Server setup-------------------*/
	char *port = config_get_string_value(config, "KERNEL_SCHEDULER_PORT");
	
	int server_fd = server_start(port, logger);

	free(port);

	if (server_fd == -1) {
		log_error(logger, "No se pudo iniciar el servidor");
		return EXIT_FAILURE;
	}

	log_info(logger, "Kernel Scheduler iniciado en el puerto %s, esperando conexiones...", config_get_string_value(config, "KERNEL_SCHEDULER_PORT"));

	/*-------------------Handle connections-------------------*/
	while(1) {
		int new_client_fd = server_client_wait(server_fd);

		if (new_client_fd == -1) {
            log_error(logger, "No se pudo aceptar la conexión");
            continue;
        }

		log_debug(logger, "Nuevo cliente conectado: %d", new_client_fd);

        int *fd_for_thread = malloc(sizeof(int));
        *fd_for_thread = new_client_fd;

        pthread_t thread;
        pthread_create(&thread, NULL, client_handler_selector, (void*)fd_for_thread);
        pthread_detach(thread);
	}
	
	return EXIT_SUCCESS;
}

int kernel_memory_connection (t_log *logger, t_config *config, char *process0) {
	char *kernel_memory_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
	char *kernel_memory_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");

	log_debug(logger, "Intentando conexión con ip: %s, puerto: %s", kernel_memory_ip, kernel_memory_port);
	kernel_memory_fd = connection_create(kernel_memory_ip, kernel_memory_port, logger);

	if (kernel_memory_fd == -1) {
		log_error(logger, "No se pudo conectar con Kernel Memory");
		return EXIT_FAILURE;
	}

	t_module_id_send (kernel_memory_fd, MODULE_KERNEL_SCHEDULER, logger);

	log_info(logger, "## Conectado a Kernel Memory");
	
	log_debug(logger, "Inicializando proceso0 con ruta: %s", process0);

	int init_result = long_term_scheduler(process0, 0);
	if (init_result == EXIT_SUCCESS) {
		log_debug(logger, "Proceso0 creado correctamente");
	} else {
		log_error(logger, "No se pudo crear proceso0");
	}

	pthread_t thread;
	pthread_create(&thread, NULL, kernel_memory_handler, NULL);
	pthread_detach(thread);

	free(kernel_memory_ip);
	free(kernel_memory_port);
	return EXIT_SUCCESS;
}

void *client_handler_selector (void *fd_ptr) {
	int client_fd = *(int *)fd_ptr; //trato fd_ptr como puntero a int y obtengo el valor para client_fd
	free(fd_ptr);

	t_module_id module_id = handshake_receiver(client_fd);

	switch (module_id) {
		case MODULE_CPU:
			log_debug(logger, "Nuevo cliente es del tipo CPU");
			cpu_handler(client_fd);
			break;

		case MODULE_IO:
			log_debug(logger, "Nuevo cliente es del tipo IO");
			io_handler(client_fd);
			break;

		default:
            log_warning(logger, "Modulo desconocido: %d", module_id);
            close(client_fd);
            return NULL;
	}

	return NULL;
}

t_log *start_logger(t_config *config) {
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("log.log", "Kernel_Scheduler", 1, level);
	return logger;
}

void *shutdown_handler (void *arg) { // Waits for the shutdown signal and performs cleanup
	sem_wait(&shutdown_sem);
	log_info(logger, "Señal de apagado recibida, apagando...");

	log_destroy(logger);
    config_destroy(config);
	pthread_mutex_destroy(&scheduler_mutex);
	pthread_mutex_destroy(&list_mutex_mutex);
	pthread_mutex_destroy(&io_mutex);
	pthread_mutex_destroy(&cpu_id_mutex);
	pthread_mutex_destroy(&io_id_mutex);
	sem_destroy(&short_term_scheduler_sem);
	sem_destroy(&shutdown_sem);
	temporal_destroy(system_timer);

	destroy_list_of_clients(list_cpu);
	destroy_list_of_clients(list_io_sleep);
	destroy_list_of_clients(list_io_stdin);
	destroy_list_of_clients(list_io_stdout);

	destroy_list_of_mutexes(list_mutexes);

	destroy_list_of_processes(list_processes);
	destroy_list_of_processes(ready_queue);
	destroy_list_of_processes(exec_processes);

	// Should add cleanup to everything that arises

	exit(EXIT_SUCCESS);
}
