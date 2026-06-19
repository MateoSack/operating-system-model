#include "memory_utils.h"

t_segment_result segment_result_deserialize(void *buffer, int *offset) { // Deserializes a t_segment_result from a buffer, updating the offset
	int size;
	t_segment_result value;
	memcpy(&size, buffer + *offset, sizeof(int));
	*offset += sizeof(int);
	memcpy(&value, buffer + *offset, size);
	*offset += size;
	return value;
}
