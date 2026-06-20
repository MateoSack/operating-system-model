#include "memory_manage.h"

extern t_list *list_processes;
extern pthread_mutex_t list_processes_mutex;
extern uint32_t total_memory_size;
extern t_log *logger;
extern t_client_info *kernel_scheduler;
extern uint32_t target_pid;
extern sem_t compaction_sem;
extern t_list *list_memory_stick;
extern pthread_mutex_t list_memory_stick_mutex;

// ver de sacarla de aca
static bool compare_by_base(void *a, void *b) {
    return ((t_hole *)a)->base < ((t_hole *)b)->base;
}


static uint32_t target_segment_id;
static bool find_by_segment_id(void *element) {
    t_segment *seg = (t_segment *)element;
    return seg->segment_id == target_segment_id;
}

t_list *get_free_holes(void) {
    t_list *occupied = list_create();

    pthread_mutex_lock(&list_processes_mutex);
    for (int i = 0; i < list_size(list_processes); i++) {
        t_pcb *pcb = list_get(list_processes, i);
        pthread_mutex_lock(&pcb->mutex);
        for (int j = 0; j < list_size(pcb->segment_table); j++) {
            t_segment *seg = list_get(pcb->segment_table, j);
            t_hole *occ    = malloc(sizeof(t_hole));
            occ->base      = seg->base;
            occ->size      = seg->size;
            list_add(occupied, occ);
        }
        pthread_mutex_unlock(&pcb->mutex);
    }
    pthread_mutex_unlock(&list_processes_mutex);

    list_sort(occupied, compare_by_base);

    t_list  *holes = list_create();
    uint32_t cursor = 0;

    for (int i = 0; i < list_size(occupied); i++) { // get all holes between occupied segments
        t_hole *occ = list_get(occupied, i);
        if (occ->base > cursor) {
            t_hole *hole = malloc(sizeof(t_hole));
            hole->base   = cursor;
            hole->size   = occ->base - cursor;
            list_add(holes, hole);
        }
        cursor = occ->base + occ->size; // skip the occupied space
    }

    if (cursor < total_memory_size) { // add a final hole if there's free space after the last occupied segment
        t_hole *hole = malloc(sizeof(t_hole));
        hole->base   = cursor;
        hole->size   = total_memory_size - cursor;
        list_add(holes, hole);
    }

    list_destroy_and_destroy_elements(occupied, free);
    return holes;
}

t_hole *best_fit(t_list *holes, uint32_t size) { // the best fit is the smallest hole that can fit the segment
    t_hole *best = NULL;
    for (int i = 0; i < list_size(holes); i++) {
        t_hole *hole = list_get(holes, i);
        if (hole->size >= size) { // if the hole is big enough
            if (best == NULL || hole->size < best->size)
                best = hole;
        }
    }
    return best;
}

t_hole *worst_fit(t_list *holes, uint32_t size) { // the worst fit is the biggest hole that can fit the segment
    t_hole *worst = NULL;
    for (int i = 0; i < list_size(holes); i++) {
        t_hole *hole = list_get(holes, i);
        if (hole->size >= size) { // if the hole is big enough
            if (worst == NULL || hole->size > worst->size)
                worst = hole;
        }
    }
    return worst;
}

t_hole *select_hole(t_list *holes, uint32_t size) {
    if (strcmp(allocation_strategy, "BEST") == 0) {
        return best_fit(holes, size);
    } else {
        return worst_fit(holes, size);
    }
}

void compact_memory(void) {
    log_debug(logger, "Inicio de compactación");

    pthread_mutex_lock(&list_processes_mutex);
    uint32_t cursor = 0;
    for (int i = 0; i < list_size(list_processes); i++) {
        t_pcb *pcb = list_get(list_processes, i);
        for (int j = 0; j < list_size(pcb->segment_table); j++) {
            t_segment *seg = list_get(pcb->segment_table, j);

            if (seg->base != cursor) {
                // Read data and write it to the new position continuously, then update the segment's base
                pthread_mutex_unlock(&list_processes_mutex);
                void *data = memory_read(seg->base, seg->size);
                if (data != NULL) {
                    memory_write(cursor, data, seg->size);
                    free(data);
                }
                pthread_mutex_lock(&list_processes_mutex);

                seg->base = cursor;
            }

            cursor += seg->size;
        }
    }
    pthread_mutex_unlock(&list_processes_mutex);

    
    log_debug(logger, "Fin de compactación");
}

void request_and_compact(void) { // Send compaction request to Kernel Scheduler and wait for confirmation, then perform compaction
    t_package *pkg = package_create();
    pkg->op_code   = COMPACTION_REQUEST;
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);

    sem_wait(&compaction_sem);

    compact_memory();

    t_package *pkg_finished = package_create();
    pkg_finished->op_code = COMPACTION_FINISHED;
    package_send(pkg_finished, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg_finished);

    log_debug(logger, "Notificación de compactación terminada enviada al Kernel Scheduler");
}

t_segment_result segment_create(uint32_t pid, uint32_t segment_id, uint32_t size) {
    t_list *holes = get_free_holes();

    uint32_t total_free_space = 0;
    for (int i = 0; i < list_size(holes); i++) {
        total_free_space += ((t_hole *)list_get(holes, i))->size;
    }
    if (total_free_space < size) {
        list_destroy_and_destroy_elements(holes, free);
        return SEGMENT_NO_SPACE;
    }

    // Try to find continuous space for the segment
    t_hole *chosen = select_hole(holes, size);

    if (chosen == NULL) { // No continuous space available, need to compact
        list_destroy_and_destroy_elements(holes, free);

        request_and_compact();

        holes  = get_free_holes();
        chosen = select_hole(holes, size);

        if (chosen == NULL) {
            // No debería pasar, pero por las dudas
            list_destroy_and_destroy_elements(holes, free);
            log_error(logger, "segment_create: No se pudo encontrar espacio para el segmento PID %u - Segment ID %u - Size %u incluso después de compactar", pid, segment_id, size);
            return SEGMENT_ERROR;
        }
    }

    // Create the segment in the chosen hole
    t_segment *seg  = malloc(sizeof(t_segment));
    seg->segment_id = segment_id;
    seg->base       = chosen->base;
    seg->size       = size;

    pthread_mutex_lock(&list_processes_mutex);
    target_pid = pid;
    t_pcb *pcb = list_find(list_processes, find_by_pid);
    pthread_mutex_unlock(&list_processes_mutex);
    if (pcb == NULL) {
        log_error(logger, "segment_create: PID %u no encontrado", pid);
        free(seg);
        list_destroy_and_destroy_elements(holes, free);
        return SEGMENT_ERROR;
    }

    pthread_mutex_lock(&pcb->mutex);
    list_add(pcb->segment_table, seg);
    pthread_mutex_unlock(&pcb->mutex);

    list_destroy_and_destroy_elements(holes, free);
    return SEGMENT_OK;
}

t_memory_stick_info *get_memory_stick_by_address(uint32_t physical_address, uint32_t *local_offset) { // Returns the memory stick that corresponds to the given physical address and calculates the local offset within that stick, NULL if out of bounds
    uint32_t cursor = 0;
    pthread_mutex_lock(&list_memory_stick_mutex);
    for (int i = 0; i < list_size(list_memory_stick); i++) {
        t_memory_stick_info *ms = list_get(list_memory_stick, i);
        if (physical_address < cursor + ms->size) {
            *local_offset = physical_address - cursor;
            pthread_mutex_unlock(&list_memory_stick_mutex);
            return ms;
        }
        cursor += ms->size;
    }
    pthread_mutex_unlock(&list_memory_stick_mutex);
    return NULL;
}

void *memory_read(uint32_t physical_address, uint32_t size) {
    void *result = malloc(size);
    if (result == NULL) return NULL;

    uint32_t bytes_done = 0;

    while (bytes_done < size) {
        uint32_t local_offset;
        t_memory_stick_info *ms = get_memory_stick_by_address(physical_address + bytes_done, &local_offset);

        if (ms == NULL) {
            log_error(logger, "memory_read: dirección %u fuera de rango", physical_address + bytes_done);
            free(result);
            return NULL;
        }

        // Number of bytes we can read from this stick starting at local_offset
        uint32_t available_in_stick = ms->size - local_offset;
        uint32_t remaining = size - bytes_done;
        uint32_t chunk;
        if (remaining < available_in_stick) {
            chunk = remaining;        // This iteration we can read all the remaining bytes (will be the last iteration)
        } else {
            chunk = available_in_stick;  // The segment is in multiple memory sticks (will iterate again)
        }

        t_package *pkg = package_create();
        pkg->op_code = MS_READ;
        package_add(pkg, &local_offset, sizeof(uint32_t));
        package_add(pkg, &chunk, sizeof(uint32_t));
        package_send(pkg, ms->fd, &ms->mutex);
        package_delete(pkg);

        sem_wait(&ms->response_sem);
        pthread_mutex_lock(&ms->mutex);
        void *chunk_data = ms->last_read_buffer;
        ms->last_read_buffer = NULL;
        pthread_mutex_unlock(&ms->mutex);

        char *data_str = bytes_to_safe_string(chunk_data, chunk);
        log_debug(logger, "memory_read: Dato leido: %s", data_str);
        free(data_str);

        if (chunk_data == NULL) {
            log_error(logger, "memory_read: chunk NULL en MS id=%d", ms->id);
            free(result);
            return NULL;
        }

        memcpy(result + bytes_done, chunk_data, chunk);
        free(chunk_data);
        bytes_done += chunk;
    }

    return result;
}

bool memory_write(uint32_t physical_address, void *data, uint32_t size) {
    uint32_t bytes_done = 0;

    while (bytes_done < size) {
        uint32_t local_offset;
        t_memory_stick_info *ms = get_memory_stick_by_address(physical_address + bytes_done, &local_offset);

        if (ms == NULL) {
            log_error(logger, "memory_write: dirección %u fuera de rango", physical_address + bytes_done);
            return false;
        }

        uint32_t available_in_stick = ms->size - local_offset;
        uint32_t remaining = size - bytes_done;
        uint32_t chunk;
        if (remaining < available_in_stick) {
            chunk = remaining;
        } else {
            chunk = available_in_stick;
        }

        t_package *pkg = package_create();
        pkg->op_code = MS_WRITE;
        package_add(pkg, &local_offset, sizeof(uint32_t));
        package_add(pkg, &chunk, sizeof(uint32_t));
        package_add(pkg, data + bytes_done, chunk); // Only writes bytes that enter in the current memory stick, if iterates multiple times will write continuisly and won't repeat data using data+bytes_done
        package_send(pkg, ms->fd, &ms->mutex);
        package_delete(pkg);

        sem_wait(&ms->response_sem);
        pthread_mutex_lock(&ms->mutex);
        bool chunk_ok = (ms->last_op_result == MS_WRITE_RESPONSE);
        ms->last_op_result = -1;
        pthread_mutex_unlock(&ms->mutex);

        if (!chunk_ok) {
            log_error(logger, "memory_write: falló escritura en MS id=%d desde el offset=%u con chunk=%u", ms->id, local_offset, chunk);
            return false;
        }

        bytes_done += chunk;
    }

    return true;
}

int segment_delete(uint32_t pid, uint32_t segment_id) {
    pthread_mutex_lock(&list_processes_mutex);
    target_pid = pid;
    t_pcb *pcb = list_find(list_processes, find_by_pid);
    if (pcb == NULL) {
        pthread_mutex_unlock(&list_processes_mutex);
        log_error(logger, "segment_delete: PID %u no encontrado", pid);
        return SEGMENT_ERROR;
    }

    // Lock del PCB antes de soltar el lock de la lista por si se intenta eliminar el pcb mientras estamos accediendo
    pthread_mutex_lock(&pcb->mutex);
    pthread_mutex_unlock(&list_processes_mutex);

    target_segment_id = segment_id;
    t_segment *segment_to_delete = list_find(pcb->segment_table, find_by_segment_id);
    if (segment_to_delete == NULL) {
        pthread_mutex_unlock(&pcb->mutex);
        log_error(logger, "segment_delete: Segmento ID %u no encontrado para PID %u", segment_id, pid);
        return SEGMENT_ERROR;
    }

    int index = -1;
    for (int i = 0; i < list_size(pcb->segment_table); i++) {
        if (list_get(pcb->segment_table, i) == segment_to_delete) {
            index = i;
            break;
        }
    }
    if (index >= 0) {
        list_remove_and_destroy_element(pcb->segment_table, index, free);
    } else {
        pthread_mutex_unlock(&pcb->mutex);
        log_error(logger, "segment_delete: inconsistencia al eliminar segmento %u para PID %u", segment_id, pid);
        return SEGMENT_ERROR;
    }

    pthread_mutex_unlock(&pcb->mutex);
    log_info(logger, "Segmento ID %u eliminado para PID %u", segment_id, pid);
    return SEGMENT_OK;
}

void send_segment_result(uint32_t pid, uint32_t segment_id, int result) {
    t_package *pkg = package_create();
    pkg->op_code = SEGMENT_RESULT;
    package_add(pkg, &pid, sizeof(uint32_t));
    package_add(pkg, &segment_id, sizeof(uint32_t));
    package_add(pkg, &result, sizeof(int));
    package_send(pkg, kernel_scheduler->fd, &kernel_scheduler->network_mutex);
    package_delete(pkg);
}
