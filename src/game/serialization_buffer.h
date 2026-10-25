#ifndef ARCANUM_SERIALIZATION_BUFFER_H_
#define ARCANUM_SERIALIZATION_BUFFER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

typedef struct ReadBuffer {
    const uint8_t* cursor;
    size_t remaining;
    bool failed;
} ReadBuffer;

static inline const uint8_t* read_buffer_take(ReadBuffer* buffer, size_t size)
{
    if (buffer == NULL) return NULL;
    if (buffer->failed || size > buffer->remaining || (size != 0 && buffer->cursor == NULL)) {
        buffer->failed = true;
        return NULL;
    }
    const uint8_t* bytes = buffer->cursor;
    if (size != 0) buffer->cursor += size;
    buffer->remaining -= size;
    return bytes;
}

static inline bool read_buffer_read(ReadBuffer* buffer, void* output, size_t size)
{
    if (buffer == NULL) return false;
    if (size != 0 && output == NULL) {
        buffer->failed = true;
        return false;
    }
    const uint8_t* bytes = read_buffer_take(buffer, size);
    if (buffer->failed) return false;
    if (size != 0) memcpy(output, bytes, size);
    return true;
}

static inline bool serialization_buffer_capacity(size_t capacity, size_t used, size_t extra, size_t* result)
{
    if (used > capacity || capacity > INT_MAX || extra > (size_t)INT_MAX - used) return false;
    size_t needed = used + extra;
    size_t next = capacity < 256 ? 256 : capacity;
    while (next < needed) next = next > INT_MAX / 2 ? INT_MAX : next * 2;
    *result = next;
    return true;
}

#endif
