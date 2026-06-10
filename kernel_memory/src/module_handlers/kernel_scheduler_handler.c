#include "kernel_scheduler_handler.h"

extern t_list *list_processes;
extern pthread_mutex_t list_processes_mutex;
extern uint32_t target_pid;
extern sem_t compaction_sem;

int kernel_scheduler_handler(t_log *logger, int client_fd, t_config *config) {
    char *base_path = config_get_string_value(config, "SCRIPTS_BASEPATH");
    
    while (1) {
        int op = operation_receive(client_fd);
        if (op == -1) {
            log_error(logger, "Kernel Scheduler disconnected");
            break;
        }

        switch (op) {
            case PROCESS_CREATE: {
                uint32_t pid = uint32_decode(client_fd);
                char *relative_path = message_receive(logger, client_fd);
            
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
                list_add(list_processes, pcb);
            
                break;
            }

            case PROCESS_END: {
                uint32_t pid = uint32_decode(client_fd);
                log_info(logger, "Ending process PID:%u", pid);
                
                pthread_mutex_lock(&list_processes_mutex);
                target_pid = pid;
                t_pcb *pcb_to_remove = list_find(list_processes, find_by_pid);
                pthread_mutex_unlock(&list_processes_mutex);
                if (pcb_to_remove != NULL) {
                    //TODO: Liberar recursos del proceso (segmentos, etc)
                    list_remove_element(list_processes, pcb_to_remove);
                    free(pcb_to_remove);
                    log_info(logger, "Process PID:%u ended correctly", pid);
                } else {
                    log_warning(logger, "Process PID:%u not found in list", pid);
                }
                
                break;
            }

            case SEGMENT_CREATE: {
                uint32_t pid = uint32_decode(client_fd);
                uint32_t segment_id = uint32_decode(client_fd);
                uint32_t segment_size = uint32_decode(client_fd);
                log_debug(logger, "Received SEGMENT_CREATE request for PID %u - Segment ID %u - Size %u", pid, segment_id, segment_size);

                t_segment_result result = segment_create(pid, segment_id, segment_size, config);

                // =============================================================
                // VER SI ES NECESARIO ENVIARLE EL RESULTADO AL KERNEL SCHEDULER
                // =============================================================
                
                switch (result) {
                    case SEGMENT_OK:
                        log_info(logger, "## PID: %u - Segmento Creado %u - Tamaño: %u", pid, segment_id, segment_size);
                        break;
                    case SEGMENT_NO_SPACE:
                        log_warning(logger, "PID: %u - No hay espacio para crear segmento %u de tamaño %u", pid, segment_id, segment_size);
                        break;
                    case SEGMENT_ERROR:
                        log_error(logger, "No se pudo crear el segmento");
                        break;
                }
                break;
            }

            case COMPACTION_READY: {
            // Kernel Scheduler notifies that compaction is ready, so we can proceed with it
            log_debug(logger, "Received COMPACTION_READY from Kernel Scheduler");
            sem_post(&compaction_sem);
            break;
            }

            case IO_MEMORY_READ: { // TODO: Volver esto un paquete (mismo paquete en IO_MEMORY_READy IO_MEMORY_WRITE)
                uint32_t pid = uint32_decode(client_fd);
                uint32_t logical_address = uint32_decode(client_fd);
                uint32_t size = uint32_decode(client_fd);
                //TODO: Implementar traduccion de direccion de logica a fisica y lectura de memoria
                log_info(logger, "Received PID %u IO_MEMORY_READ request for physical address %u with size %u", pid, logical_address, size); // cambiar address
                message_send("OK", client_fd); 
                uint32_send(client_fd, 777);
                break;
            }

            case IO_MEMORY_WRITE: { // TODO: Volver esto un paquete
                uint32_t pid = uint32_decode(client_fd);
                uint32_t logical_address = uint32_decode(client_fd);
                uint32_t size = uint32_decode(client_fd);
                log_info(logger, "Received PID %u IO_MEMORY_WRITE request for physical address %u with size %u", pid, logical_address, size); // cambiar address
                message_send("OK", client_fd);
                uint32_send(client_fd, 777);
                break;
            }
        }
    }
    return EXIT_FAILURE;
}
