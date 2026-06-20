#include "kernel_scheduler_handler.h"

extern t_list *list_processes;
extern pthread_mutex_t list_processes_mutex;
extern uint32_t target_pid;
extern sem_t compaction_sem;
extern t_client_info *kernel_scheduler;

int kernel_scheduler_handler(t_log *logger, int client_fd, t_config *config) {
    char *base_path = config_get_string_value(config, "SCRIPTS_BASEPATH");
    
    while (1) {
        int op = operation_receive(client_fd);
        if (op == -1) {
            log_error(logger, "Kernel Scheduler desconectado");
            break;
        }

        switch (op) {
            case PROCESS_CREATE: {
                int size;
                int offset = 0;
                void *buffer = buffer_receive(&size, client_fd);
                if (buffer == NULL) {
                    log_error(logger, "Fallo al recibir payload de PROCESS_CREATE");
                    break;
                }
                uint32_t pid = uint32_deserialize(buffer, &offset);
                char *relative_path = string_deserialize(buffer, &offset);
                free(buffer);

                // Build full path and create PCB
                char *full_path = string_from_format("%s/%s", base_path, relative_path);
                free(relative_path);
                log_info(logger, "## PID: %u - Proceso Creado", pid);
                
                t_pcb *pcb = create_pcb(pid, full_path);
                if (pcb->instructions == NULL) {
                    log_error(logger, "No se pudieron cargar las instrucciones para PID %u - el archivo puede no existir: %s", pid, full_path);
                    free(full_path);
                    break;
                }

                log_debug(logger, "La primera instruccion del proceso PID %u es: %s", pid, pcb->instructions[0]);
                free(full_path);
                pthread_mutex_lock(&list_processes_mutex);
                list_add(list_processes, pcb);
                pthread_mutex_unlock(&list_processes_mutex);
            
                break;
            }

            case PROCESS_END: {
                uint32_t pid = uint32_decode(client_fd);
                log_info(logger, "Finalizando proceso - PID:%u", pid);
                
                pthread_mutex_lock(&list_processes_mutex);
                target_pid = pid;
                t_pcb *pcb_to_remove = list_find(list_processes, find_by_pid);
                if (pcb_to_remove != NULL) {
                    pthread_mutex_lock(&pcb_to_remove->mutex);
                    // Remove from list while holding pcb
                    list_remove_element(list_processes, pcb_to_remove);
                    pthread_mutex_unlock(&list_processes_mutex);

                    if (pcb_to_remove->segment_table != NULL) {
                        list_destroy_and_destroy_elements(pcb_to_remove->segment_table, free);
                    }
                    if (pcb_to_remove->instructions != NULL) {
                        for (int i = 0; pcb_to_remove->instructions[i] != NULL; i++) free(pcb_to_remove->instructions[i]);
                        free(pcb_to_remove->instructions);
                    }

                    pthread_mutex_unlock(&pcb_to_remove->mutex);
                    pthread_mutex_destroy(&pcb_to_remove->mutex);
                    free(pcb_to_remove);
                    log_info(logger, "Process PID:%u ended correctly", pid);
                } else {
                    pthread_mutex_unlock(&list_processes_mutex);
                    log_warning(logger, "Process PID:%u not found in list", pid);
                }
                
                break;
            }

            case SEGMENT_CREATE: {
                int size;
                int offset = 0;
                void *buffer = buffer_receive(&size, client_fd);
                if (buffer == NULL) {
                    log_error(logger, "Fallo al recibir payload de SEGMENT_CREATE");
                    break;
                }
                uint32_t pid = uint32_deserialize(buffer, &offset);
                uint32_t segment_id = uint32_deserialize(buffer, &offset);
                uint32_t segment_size = uint32_deserialize(buffer, &offset);
                free(buffer);
                log_debug(logger, "Pedido de SEGMENT_CREATE para PID %u - Segmento ID %u - Tamaño %u", pid, segment_id, segment_size);

                t_segment_result result = segment_create(pid, segment_id, segment_size);
                
                switch (result) {
                    // Send result to scheduler
                    case SEGMENT_OK:
                        log_info(logger, "## PID: %u - Segmento Creado %u - Tamaño: %u", pid, segment_id, segment_size);
                        send_segment_result(pid, segment_id, SEGMENT_OK);
                        break;
                    case SEGMENT_NO_SPACE:
                        log_warning(logger, "PID: %u - No hay espacio para crear segmento %u de tamaño %u", pid, segment_id, segment_size);
                        send_segment_result(pid, segment_id, SEGMENT_NO_SPACE);
                        break;
                    case SEGMENT_ERROR:
                        log_error(logger, "No se pudo crear el segmento");
                        send_segment_result(pid, segment_id, SEGMENT_ERROR);
                        break;
                }
                break;
            }

            case SEGMENT_DELETE: {
                int size;
                int offset = 0;
                void *buffer = buffer_receive(&size, client_fd);
                if (buffer == NULL) {
                    log_error(logger, "Fallo al recibir payload de SEGMENT_DELETE");
                    break;
                }
                uint32_t pid = uint32_deserialize(buffer, &offset);
                uint32_t segment_id = uint32_deserialize(buffer, &offset);
                free(buffer);
                log_debug(logger, "Pedido de SEGMENT_DELETE para PID %u - Segmento ID %u", pid, segment_id);

                t_segment_result result = segment_delete(pid, segment_id);

                switch (result) { // VER DE MANDAR LOS RESULTADOS A KS
                    case SEGMENT_OK:
                        log_debug(logger, "PID: %u - Segmento Eliminado %u", pid, segment_id);
                        send_segment_result(pid, segment_id, SEGMENT_OK);
                        break;
                    case SEGMENT_ERROR:
                        log_error(logger, "No se pudo eliminar el segmento");
                        send_segment_result(pid, segment_id, SEGMENT_ERROR);

                        break;
                    default:
                        log_error(logger, "Resultado de segment_delete no esperado");
                        send_segment_result(pid, segment_id, SEGMENT_ERROR);
                        break;
                }
                break;
            }

            case COMPACTION_READY: {
                // Kernel Scheduler notifies that compaction is ready, so we can proceed with it
                log_debug(logger, "Aviso de COMPACTION_READY recibido");
                sem_post(&compaction_sem);
                break;
            }

            case IO_MEMORY_READ: {
                log_debug(logger, "Pedido de IO_MEMORY_READ recibido");
                int size;
                int offset = 0;
                void *buffer = buffer_receive(&size, client_fd);
                if (buffer == NULL) {
                    log_error(logger, "Fallo al recibir payload de IO_MEMORY_READ");
                    break;
                }

                uint32_t pid = uint32_deserialize(buffer, &offset);
                uint32_t physical_address = uint32_deserialize(buffer, &offset);
                uint32_t size_to_read = uint32_deserialize(buffer, &offset);
                free(buffer);

                log_debug(logger, "Leyendo direccion: %u - Tamaño a leer: %u", physical_address, size_to_read);
                char *data = memory_read(physical_address, size_to_read);
                if (data == NULL) {
                    log_error(logger, "Error al leer memoria para PID %u - Dir. Física: %u - Tamaño: %u", pid, physical_address, size_to_read);
                    // Send error response to Kernel Scheduler
                    t_package *err_pkg = package_create();
                    err_pkg->op_code = IO_MEMORY_READ;
                    package_add(err_pkg, &pid, sizeof(uint32_t));
                    //Mandar cadena vacia para indicar error
                    char *error_data = "";
                    package_string_add(err_pkg, error_data);
                    package_send(err_pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
                    package_delete(err_pkg);
                    break;
                }
                log_debug(logger, "Valor leido: %s", data);
                // Send read data back to Kernel Scheduler
                t_package *pkg = package_create();
                pkg->op_code = IO_MEMORY_READ;
                package_add(pkg, &pid, sizeof(uint32_t));
                package_string_add(pkg, data);
                package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
                package_delete(pkg);
                log_info(logger, "## PID: %u - Lectura - Dir. Física: %u - Tamaño: %u", pid, physical_address, size);
                free(data);
                break;
            }

            case IO_MEMORY_WRITE: {
                log_debug(logger, "Pedido de IO_MEMORY_WRITE recibido");
                int size;
                int offset = 0;
                void *buffer = buffer_receive(&size, client_fd);
                if (buffer == NULL) {
                    log_error(logger, "Fallo al recibir payload de IO_MEMORY_READ");
                    break;
                }

                uint32_t pid = uint32_deserialize(buffer, &offset);
                uint32_t physical_address = uint32_deserialize(buffer, &offset);
                uint32_t size_to_write = uint32_deserialize(buffer, &offset);
                char *raw_data = string_deserialize(buffer, &offset);
                free(buffer);

                char *padded_data = calloc(size_to_write, 1); // Clean memory with \0

                size_t bytes_to_copy = strlen(raw_data);
                if (bytes_to_copy > size_to_write) { // If sended more than size to write
                    bytes_to_copy = size_to_write; 
                }
                memcpy(padded_data, raw_data, bytes_to_copy);

                free(raw_data);

                log_debug(logger, "Datos a escribir: %s, con tamaño: %d", padded_data, size_to_write);

                t_package *pkg = package_create();
                pkg->op_code = IO_MEMORY_WRITE;
                package_add(pkg, &pid, sizeof(uint32_t));

                bool couldWrite = memory_write(physical_address, padded_data, size_to_write);

                if (!couldWrite) {
                    log_error(logger, "Error al escribir memoria para PID %u - Dir. Física: %u - Tamaño: %u", pid, physical_address, size_to_write);
                } else {
                    log_info(logger, "## PID: %u - Escritura - Dir. Física: %u - Tamaño: %u", pid, physical_address, size_to_write);
                }

                package_add(pkg, &couldWrite, sizeof(bool));
                package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
                package_delete(pkg);
                free(padded_data);
                break;
            }
        }
    }
    return EXIT_FAILURE;
}
