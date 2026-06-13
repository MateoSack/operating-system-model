#include "cpu_handler.h"

extern t_list *list_cpu;
extern t_list *list_processes;

extern pthread_mutex_t list_processes_mutex;
extern uint32_t target_pid;

// Helper function to receive a single uint32_t from a package
static uint32_t receive_uint32_from_package(int client_fd) {
    int buffer_size;
    recv(client_fd, &buffer_size, sizeof(int), MSG_WAITALL);
    
    void *buffer = malloc(buffer_size);
    recv(client_fd, buffer, buffer_size, MSG_WAITALL);
    
    int offset = 0;
    int size;
    memcpy(&size, buffer + offset, sizeof(int));
    offset += sizeof(int);
    uint32_t value;
    memcpy(&value, buffer + offset, size);
    
    free(buffer);
    return value;
}

// Helper function to receive two uint32_t values from a package
static void receive_two_uint32_from_package(int client_fd, uint32_t *pid, uint32_t *pc) {
    int buffer_size;
    recv(client_fd, &buffer_size, sizeof(int), MSG_WAITALL);
    
    void *buffer = malloc(buffer_size);
    recv(client_fd, buffer, buffer_size, MSG_WAITALL);
    
    int offset = 0;
    int pid_size;
    memcpy(&pid_size, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(pid, buffer + offset, pid_size);
    offset += pid_size;
    
    int pc_size;
    memcpy(&pc_size, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(pc, buffer + offset, pc_size);
    
    free(buffer);
}

int cpu_handler (t_log *logger, t_client_info *cpu) {
    while (1) {
        int op = operation_receive(cpu->fd);
        if (op == -1) {
            remove_client_from_list(list_cpu, cpu);
            destroy_client(cpu);
            
            log_error(logger, "CPU %d disconnected", cpu->id);

            //Llamar función de fallo y shutdown
            break;
        }

        switch (op) {
            case CONTEXT_TRANSFER: {
                int size;
                int offset = 0;
                void *buffer = buffer_receive(&size, cpu->fd);
                
                uint32_t pid = uint32_deserialize(buffer, &offset);
                
                t_cpu_context *ctx = malloc(sizeof(t_cpu_context));
                ctx->pc  = uint32_deserialize(buffer, &offset);
                ctx->ax  = uint8_deserialize(buffer, &offset);
                ctx->bx  = uint8_deserialize(buffer, &offset);
                ctx->cx  = uint8_deserialize(buffer, &offset);
                ctx->dx  = uint8_deserialize(buffer, &offset);
                ctx->eax = uint32_deserialize(buffer, &offset);
                ctx->ebx = uint32_deserialize(buffer, &offset);
                ctx->ecx = uint32_deserialize(buffer, &offset);
                ctx->edx = uint32_deserialize(buffer, &offset);
                ctx->si  = uint32_deserialize(buffer, &offset);
                ctx->di  = uint32_deserialize(buffer, &offset);
                free(buffer);

                log_info(logger, "Received context transfer for PID %d from CPU %d", pid, cpu->id);
                
                pthread_mutex_lock(&list_processes_mutex);
                target_pid = pid;
                t_pcb *pcb = list_find(list_processes, find_by_pid);
                pthread_mutex_unlock(&list_processes_mutex);
                if (pcb == NULL) {
                    log_error(logger, "Process with PID %d not found", pid);
                    free(ctx);
                    break;
                }
                pcb->context = *ctx;
                free(ctx);
                log_info(logger, "Context updated correctly for PID %d", pid);
                break;
            }

            case CONTEXT_SEEK: {
                uint32_t pid = receive_uint32_from_package(cpu->fd);
                log_info(logger, "Received context seek request for PID %d from CPU %d", pid, cpu->id);
                
                pthread_mutex_lock(&list_processes_mutex);
                target_pid = pid;
                t_pcb *pcb = list_find(list_processes, find_by_pid);
                pthread_mutex_unlock(&list_processes_mutex);
                if (pcb == NULL) {
                    log_error(logger, "Process with PID %d not found", pid);
                    break;
                }
                context_send(&pcb->context, cpu->fd, &cpu->network_mutex);
                log_info(logger, "Context sent correctly for PID %d", pid);
                break;
            }

            case INSTRUCTION_FETCH: {
                log_debug(logger, "Pedido de INSTRUCTION_FETCH recibido de CPU");
                uint32_t pid, pc;
                receive_two_uint32_from_package(cpu->fd, &pid, &pc);
                log_debug(logger, "PID recibido: %d", pid);
                log_debug(logger, "PC recibido: %d", pc);
                
                pthread_mutex_lock(&list_processes_mutex);
                target_pid = pid;
                t_pcb *pcb = list_find(list_processes, find_by_pid);
                pthread_mutex_unlock(&list_processes_mutex);
                if (pcb == NULL) {
                    log_error(logger, "Process with PID %d not found", pid);
                    break;
                }
                
                if (pcb->instructions == NULL) {
                    log_error(logger, "ERROR: Instructions not loaded for PID %d - file may not exist", pid);
                    break;
                }
                
                char *instruction = pcb->instructions[pc];
                log_info(logger, "## PID: %u - Obtener instruccion: %u - Instruccion: %s", pid, pc, instruction);
                message_send_with_op_code(instruction, INSTRUCTION_FETCH, cpu->fd, &cpu->network_mutex);
                log_debug(logger, "Instruccion enviada correctamente a PID %d: %s", pid, instruction);
                break;
            }
        }
    }
    return -1;
}

