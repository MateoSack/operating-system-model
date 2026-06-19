#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <stdint.h>
#include <utils/process_utils.h>

typedef struct {
    int fd;
    uint32_t id;
    uint32_t size;
    pthread_mutex_t mutex;
    pthread_mutex_t network_mutex;
    sem_t response_sem;
    int last_op_result;
    void *last_read_buffer;
    int last_read_size;
} t_memory_stick_info;

typedef struct {
    uint32_t segment_id;
    uint32_t base;
    uint32_t size;
} t_segment;

typedef struct {
    uint32_t base;
    uint32_t size;
} t_hole;

typedef enum {
    SEGMENT_OK,
    SEGMENT_NO_SPACE,
    SEGMENT_ERROR,
} t_segment_result;

t_segment_result segment_result_deserialize(void *buffer, int *offset);

#endif
