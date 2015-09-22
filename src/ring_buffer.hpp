#ifndef GENESIS_RING_BUFFER_HPP
#define GENESIS_RING_BUFFER_HPP

#include "util.hpp"
#include "atomics.hpp"
#include "os.hpp"

struct RingBuffer {
    OsMirroredMemory mem;
    atomic_long write_offset;
    atomic_long read_offset;
    int capacity;
};

int ring_buffer_init(struct RingBuffer *rb, int requested_capacity);
void ring_buffer_deinit(struct RingBuffer *rb);

/// Do not write more than capacity.
char *ring_buffer_write_ptr(struct RingBuffer *ring_buffer);
/// `count` in bytes.
void ring_buffer_advance_write_ptr(struct RingBuffer *ring_buffer, int count);

/// Do not read more than capacity.
char *ring_buffer_read_ptr(struct RingBuffer *ring_buffer);
/// `count` in bytes.
void ring_buffer_advance_read_ptr(struct RingBuffer *ring_buffer, int count);

/// Returns how many bytes of the buffer is used, ready for reading.
int ring_buffer_fill_count(struct RingBuffer *ring_buffer);

/// Returns how many bytes of the buffer is free, ready for writing.
int ring_buffer_free_count(struct RingBuffer *ring_buffer);

/// Must be called by the writer.
void ring_buffer_clear(struct RingBuffer *ring_buffer);

#endif
