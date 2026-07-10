#include "cpu_handler.h"

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
                log_debug(logger, "Peticion de CONTEXT_TRANSFER recibida para PID %d desde CPU %d", pid, cpu->id);
                
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
                
                pthread_mutex_lock(&list_processes_mutex);
                target_pid = pid;
                t_pcb *pcb = list_find(list_processes, find_by_pid);
                if (pcb == NULL) {
                    pthread_mutex_unlock(&list_processes_mutex);
                    log_error(logger, "Proceso con PID %d no encontrado", pid);
                    free(ctx);
                    break;
                }
                pthread_mutex_lock(&pcb->mutex);
                pthread_mutex_unlock(&list_processes_mutex);

                pcb->context = *ctx;
                log_debug(logger, "CONTEXT_TRANSFER: pcb->context.pc actualizado a %u para PID %d", pcb->context.pc, pid);
                pthread_mutex_unlock(&pcb->mutex);
                free(ctx);
                log_debug(logger, "Contexto actualizado correctamente para PID %d", pid);
                break;
            }

            case CONTEXT_SEEK: {
                int size;
                int offset = 0;
                void *buffer = buffer_receive(&size, cpu->fd);
                if (buffer == NULL) {
                    log_error(logger, "Fallo al recibir payload de CONTEXT_SEEK");
                    break;
                }

                uint32_t pid = uint32_deserialize(buffer, &offset);
                free(buffer);
                log_debug(logger, "Pedido de CONTEXT_SEEK para PID %d desde CPU %d", pid, cpu->id);
                
                pthread_mutex_lock(&list_processes_mutex);
                target_pid = pid;
                t_pcb *pcb = list_find(list_processes, find_by_pid);
                if (pcb == NULL) {
                    log_error(logger, "Proceso con PID %d no encontrado", pid);
                    break;
                }
                pthread_mutex_lock(&pcb->mutex);
                pthread_mutex_unlock(&list_processes_mutex);
                context_send_from_pcb(pcb, pid, cpu->fd, &cpu->network_mutex);
                pthread_mutex_unlock(&pcb->mutex);
                
                log_debug(logger, "Contexto enviado correctamente para PID %d", pid);
                break;
            }

            case INSTRUCTION_FETCH: {
                log_debug(logger, "Pedido de INSTRUCTION_FETCH recibido de CPU");
                uint32_t pid, pc;
                int size;
                int offset = 0;
                void *buffer = buffer_receive(&size, cpu->fd);
                if (buffer == NULL) {
                    log_error(logger, "Fallo al recibir payload de INSTRUCTION_FETCH");
                    break;
                }
                pid = uint32_deserialize(buffer, &offset);
                pc  = uint32_deserialize(buffer, &offset);
                free(buffer);
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
                usleep(instruction_delay * 1000); // Convert milliseconds to microseconds for usleep
                message_send_with_op_code(instruction, INSTRUCTION_FETCH, cpu->fd, &cpu->network_mutex);
                log_debug(logger, "Instruccion enviada correctamente a PID %d: %s", pid, instruction);
                break;
            }
        }
    }
    return -1;
}

void context_send_from_pcb(t_pcb *pcb, uint32_t pid, int fd, pthread_mutex_t *mutex) {
    log_debug(logger, "context_send_from_pcb: enviando pc=%u para PID %d", pcb->context.pc, pid);
    t_package *pkg = package_create();
    pkg->op_code = CONTEXT_TRANSFER;
    package_add(pkg, &pcb->context.pc,  sizeof(uint32_t));
    package_add(pkg, &pcb->context.ax,  sizeof(uint8_t));
    package_add(pkg, &pcb->context.bx,  sizeof(uint8_t));
    package_add(pkg, &pcb->context.cx,  sizeof(uint8_t));
    package_add(pkg, &pcb->context.dx,  sizeof(uint8_t));
    package_add(pkg, &pcb->context.eax, sizeof(uint32_t));
    package_add(pkg, &pcb->context.ebx, sizeof(uint32_t));
    package_add(pkg, &pcb->context.ecx, sizeof(uint32_t));
    package_add(pkg, &pcb->context.edx, sizeof(uint32_t));
    package_add(pkg, &pcb->context.si,  sizeof(uint32_t));
    package_add(pkg, &pcb->context.di,  sizeof(uint32_t));
    uint32_t seg_count = list_size(pcb->segment_table);
    package_add(pkg, &seg_count, sizeof(uint32_t));
    for (int i = 0; i < seg_count; i++) {
        t_segment *seg = list_get(pcb->segment_table, i);
        package_add(pkg, &seg->segment_id, sizeof(uint32_t));
        package_add(pkg, &seg->base,       sizeof(uint32_t));
        package_add(pkg, &seg->size,       sizeof(uint32_t));
    }
    package_send(pkg, fd, mutex);
    package_delete(pkg);
}
