#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <stdint.h>

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

#endif
