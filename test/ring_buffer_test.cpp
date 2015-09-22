#include "ring_buffer_test.hpp"
#include "ring_buffer.hpp"
#include "os.hpp"

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <atomic>
using std::atomic_int;
using std::atomic_bool;

static void assert_no_err(int err) {
    if (err)
        panic("Error: %s", genesis_strerror(err));
}

static void basic_test(void) {
    RingBuffer rb;
    assert_no_err(ring_buffer_init(&rb, 10));

    assert(rb.capacity == 4096);

    char *write_ptr = ring_buffer_write_ptr(&rb);
    int amt = sprintf(write_ptr, "hello") + 1;
    ring_buffer_advance_write_ptr(&rb, amt);

    assert(ring_buffer_fill_count(&rb) == amt);
    assert(ring_buffer_free_count(&rb) == 4096 - amt);

    char *read_ptr = ring_buffer_read_ptr(&rb);

    assert(strcmp(read_ptr, "hello") == 0);

    ring_buffer_advance_read_ptr(&rb, amt);

    assert(ring_buffer_fill_count(&rb) == 0);
    assert(ring_buffer_free_count(&rb) == rb.capacity);

    ring_buffer_advance_write_ptr(&rb, 4094);
    ring_buffer_advance_read_ptr(&rb, 4094);
    amt = sprintf(ring_buffer_write_ptr(&rb), "writing past the end") + 1;
    ring_buffer_advance_write_ptr(&rb, amt);

    assert(ring_buffer_fill_count(&rb) == amt);

    assert(strcmp(ring_buffer_read_ptr(&rb), "writing past the end") == 0);

    ring_buffer_advance_read_ptr(&rb, amt);

    assert(ring_buffer_fill_count(&rb) == 0);
    assert(ring_buffer_free_count(&rb) == rb.capacity);
}

static RingBuffer *rb = nullptr;
static const int size = 3528;
static long expected_write_head;
static long expected_read_head;
static atomic_bool done;
static atomic_int write_it;
static atomic_int read_it;

static void reader_thread_run(void *) {
    while (!done) {
        read_it += 1;
        int fill_count = ring_buffer_fill_count(rb);
        assert(fill_count >= 0);
        assert(fill_count <= size);
        int amount_to_read = min(os_random_double() * 2.0 * fill_count, fill_count);
        ring_buffer_advance_read_ptr(rb, amount_to_read);
        expected_read_head += amount_to_read;
    }
}

static void writer_thread_run(void *) {
    while (!done) {
        write_it += 1;
        int fill_count = ring_buffer_fill_count(rb);
        assert(fill_count >= 0);
        assert(fill_count <= size);
        int free_count = size - fill_count;
        assert(free_count >= 0);
        assert(free_count <= size);
        int value = min(os_random_double() * 2.0 * free_count, free_count);
        ring_buffer_advance_write_ptr(rb, value);
        expected_write_head += value;
    }
}

static void threaded_test(void) {
    rb = ok_mem(allocate_zero<RingBuffer>(1));
    assert_no_err(ring_buffer_init(rb, size));
    expected_write_head = 0;
    expected_read_head = 0;
    read_it = 0;
    write_it = 0;
    done = false;

    OsThread *reader_thread;
    assert_no_err(os_thread_create(reader_thread_run, nullptr, false, &reader_thread));

    OsThread *writer_thread;
    assert_no_err(os_thread_create(writer_thread_run, nullptr, false, &writer_thread));

    while (read_it < 100000 || write_it < 100000) {}
    done = true;

    os_thread_destroy(reader_thread);
    os_thread_destroy(writer_thread);

    int fill_count = ring_buffer_fill_count(rb);
    int expected_fill_count = expected_write_head - expected_read_head;
    assert(fill_count == expected_fill_count);
}

void test_ring_buffer(void) {
    basic_test();
    threaded_test();
}
