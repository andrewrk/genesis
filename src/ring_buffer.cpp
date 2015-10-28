#include "ring_buffer.hpp"


int ring_buffer_init(struct RingBuffer *rb, int requested_capacity) {
    int err;
    if ((err = os_init_mirrored_memory(&rb->mem, requested_capacity)))
        return err;
    rb->write_offset = 0;
    rb->read_offset = 0;
    rb->capacity = rb->mem.capacity;

    return 0;
}

void ring_buffer_deinit(struct RingBuffer *rb) {
    os_deinit_mirrored_memory(&rb->mem);
}

char *ring_buffer_write_ptr(struct RingBuffer *rb) {
    return rb->mem.address + (rb->write_offset % rb->capacity);
}

void ring_buffer_advance_write_ptr(struct RingBuffer *rb, int count) {
    rb->write_offset += count;
    assert(ring_buffer_fill_count(rb) >= 0);
}

char *ring_buffer_read_ptr(struct RingBuffer *rb) {
    return rb->mem.address + (rb->read_offset % rb->capacity);
}

void ring_buffer_advance_read_ptr(struct RingBuffer *rb, int count) {
    rb->read_offset += count;
    assert(ring_buffer_fill_count(rb) >= 0);
}

int ring_buffer_fill_count(struct RingBuffer *rb) {
    // Whichever offset we load first might have a smaller value. So we load
    // the read_offset first.
    long read_offset = rb->read_offset.load();
    long write_offset = rb->write_offset.load();
    int count = write_offset - read_offset;
    assert(count >= 0);
    assert(count <= rb->capacity);
    return count;
}

int ring_buffer_free_count(struct RingBuffer *rb) {
    return rb->capacity - ring_buffer_fill_count(rb);
}

void ring_buffer_clear(struct RingBuffer *rb) {
    return rb->write_offset.store(rb->read_offset.load());
}
