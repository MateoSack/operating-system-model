#include <main.h>

t_log *logger;
t_config *config;

t_scheduler_algorithm scheduler_algorithm;
int quantum = 0;
int suspension_timeout = 0;

t_scheduler_algorithm *queue_algorithms = NULL;
int queue_algorithms_count = 1;

bool queue_preemption = false;

bool can_schedule = true;

pthread_mutex_t can_schedule_mutex = PTHREAD_MUTEX_INITIALIZER;

t_list *list_cpu = NULL;

t_list *list_io_sleep = NULL;
t_list *list_io_stdin = NULL;
t_list *list_io_stdout = NULL;

t_list *pending_request_io_sleep = NULL;
t_list *pending_request_io_stdin = NULL;
t_list *pending_io_stdin_reading = NULL; // Use for before getting data to kernel memory
t_list *pending_request_io_stdout = NULL;

pthread_mutex_t pending_io_stdin_reading_mutex = PTHREAD_MUTEX_INITIALIZER;

t_list *list_processes = NULL;
t_list **ready_queue = NULL; // In CMN, this is an array of ready queues, one per priority level. In FIFO and RR, this is a single ready queue at index 0.
t_list *exec_processes = NULL;
t_list *block_processes = NULL;
t_list *suspended_processes = NULL;

t_list *list_mutexes = NULL;

t_temporal *system_timer;

pthread_mutex_t scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_mutex_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t io_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cpu_id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t io_id_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t block_processes_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t suspended_processes_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t short_term_scheduler_sem;

sem_t shutdown_sem;

sem_t compaction_finished_sem;

t_client_info *kernel_memory = NULL;

uint32_t next_cpu_id = 0;
uint32_t next_io_id = 0;

uint32_t current_max_pid = 0;

int main(int argc, char *argv[]) {
	/*-------------------Initial Setup-------------------*/
	if (argc < 3) {
        printf("Modo de uso: %s <config_file> <first_process_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

	if (setup(argv[1]) == EXIT_FAILURE) return EXIT_FAILURE;

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

void *client_handler_selector (void *fd_ptr) { // Receives the client fd, performs the handshake to identify the module type, and calls the appropriate handler
	int client_fd = *(int *)fd_ptr;
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

t_log *start_logger(t_config *config) { // Initializes the logger based on the configuration file
	char *level_str = config_get_string_value(config, "LOG_LEVEL");
	t_log_level level = log_level_from_string(level_str);
	t_log *logger = log_create("log.log", "Kernel_Scheduler", 1, level);
	return logger;
}

int setup (char *config_path) { // Initializes the configuration, logger, scheduler algorithm, quantum, lists, semaphores, and threads for the short term scheduler and shutdown handler
	config = config_create(config_path);
	if(config == NULL) return EXIT_FAILURE;
	logger = start_logger(config);

	char *scheduler_algorithm_str = config_get_string_value(config, "PLANIFICATION_ALGORITHM");
	log_debug(logger, "Algoritmo de planificación: %s", scheduler_algorithm_str);

	scheduler_algorithm = scheduler_algorithm_from_string(scheduler_algorithm_str);
	quantum = config_get_int_value(config, "RR_QUANTUM");
	suspension_timeout = config_get_int_value(config, "SUSPENSION_TIMEOUT");

	char *queue_preemption_str = config_get_string_value(config, "QUEUE_PREEMPTION");

	if (strcmp(queue_preemption_str, "TRUE") == 0) {
		queue_preemption = true;
	}

	log_debug(logger, "Desalojo entre colas habilitado: %d", queue_preemption);

	free(scheduler_algorithm_str);

	bool has_rr = false;

	if (scheduler_algorithm == CMN) {
		char **queue_algorithms_strs = config_get_array_value(config, "QUEUES_ALGORITHMS");

		queue_algorithms_count = string_array_size(queue_algorithms_strs);

		queue_algorithms = malloc(queue_algorithms_count * sizeof(t_scheduler_algorithm));

		for (int i = 0; i < queue_algorithms_count; i++) {
			queue_algorithms[i] = scheduler_algorithm_from_string(queue_algorithms_strs[i]);
			log_debug(logger, "Algoritmo de la cola %d: %s", i, queue_algorithms_strs[i]);

			if (queue_algorithms[i] == RR) has_rr = true;

			if (queue_algorithms[i] == CMN) {
				string_array_destroy(queue_algorithms_strs);
				log_error(logger, "No se puede usar CMN como algoritmo de una cola");
				return EXIT_FAILURE;
			}
		}
		
		string_array_destroy(queue_algorithms_strs);
	}

	ready_queue = malloc(queue_algorithms_count * sizeof(t_list*));
	for (int i = 0; i < queue_algorithms_count; i++) {
		ready_queue[i] = list_create();
	}

	list_cpu = list_create();

    list_io_sleep = list_create();
	list_io_stdin = list_create();
	list_io_stdout = list_create();

	pending_request_io_sleep = list_create();
	pending_request_io_stdin = list_create();
	pending_io_stdin_reading = list_create();
	pending_request_io_stdout = list_create();

	list_processes = list_create();

	exec_processes = list_create();
	block_processes = list_create();
	suspended_processes = list_create();

	list_mutexes = list_create();

	sem_init(&short_term_scheduler_sem, 0, 0);
	sem_init(&shutdown_sem, 0, 0);
	sem_init(&compaction_finished_sem, 0, 0);

	pthread_t shutdown_thread;
	pthread_create(&shutdown_thread, NULL, shutdown_handler, NULL);
	pthread_detach(shutdown_thread);

	pthread_t short_term_scheduler_thread;
	pthread_create(&short_term_scheduler_thread, NULL, short_term_scheduler_main, NULL);
	pthread_detach(short_term_scheduler_thread);

	if (scheduler_algorithm == RR || has_rr) {
		system_timer = temporal_create();

        pthread_t quantum_thread;
        pthread_create(&quantum_thread, NULL, quantum_manager, NULL);
        pthread_detach(quantum_thread);
	}

	return EXIT_SUCCESS;
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
	for (int i = 0; i < queue_algorithms_count; i++) {
		destroy_list_of_processes(ready_queue[i]);
	}
	destroy_list_of_processes(exec_processes);

	free(queue_algorithms);
	free(ready_queue);

	// Should add cleanup to everything that arises

	exit(EXIT_FAILURE);
}
