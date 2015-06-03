#include "ring_buffer_test.hpp"
#include "ring_buffer.hpp"
#include "threads.hpp"
#include "os.hpp"

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <atomic>
using std::atomic_int;
using std::atomic_bool;

static void assert_no_err(int err) {
    if (err)
        panic("Error: %s", genesis_error_string(err));
}

static void basic_test(void) {
    RingBuffer rb(10);

    assert(rb.capacity() == 4096);

    char *write_ptr = rb.write_ptr();
    int amt = sprintf(write_ptr, "hello") + 1;
    rb.advance_write_ptr(amt);

    assert(rb.fill_count() == amt);
    assert(rb.free_count() == 4096 - amt);

    char *read_ptr = rb.read_ptr();

    assert(strcmp(read_ptr, "hello") == 0);

    rb.advance_read_ptr(amt);

    assert(rb.fill_count() == 0);
    assert(rb.free_count() == rb.capacity());

    rb.advance_write_ptr(4094);
    rb.advance_read_ptr(4094);
    amt = sprintf(rb.write_ptr(), "writing past the end") + 1;
    rb.advance_write_ptr(amt);

    assert(rb.fill_count() == amt);

    assert(strcmp(rb.read_ptr(), "writing past the end") == 0);

    rb.advance_read_ptr(amt);

    assert(rb.fill_count() == 0);
    assert(rb.free_count() == rb.capacity());
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
        int fill_count = rb->fill_count();
        assert(fill_count >= 0);
        assert(fill_count <= size);
        int amount_to_read = min(os_random_double() * 2.0 * fill_count, fill_count);
        rb->advance_read_ptr(amount_to_read);
        expected_read_head += amount_to_read;
    }
}

static void writer_thread_run(void *) {
    while (!done) {
        write_it += 1;
        int fill_count = rb->fill_count();
        assert(fill_count >= 0);
        assert(fill_count <= size);
        int free_count = size - fill_count;
        assert(free_count >= 0);
        assert(free_count <= size);
        int value = min(os_random_double() * 2.0 * free_count, free_count);
        rb->advance_write_ptr(value);
        expected_write_head += value;
    }
}

static void threaded_test(void) {
    rb = create<RingBuffer>(size);
    expected_write_head = 0;
    expected_read_head = 0;
    read_it = 0;
    write_it = 0;
    done = false;

    Thread reader_thread;
    assert_no_err(reader_thread.start(reader_thread_run, nullptr));

    Thread writer_thread;
    assert_no_err(writer_thread.start(writer_thread_run, nullptr));

    while (read_it < 100000 || write_it < 100000) {}
    done = true;

    reader_thread.join();
    writer_thread.join();

    int fill_count = rb->fill_count();
    int expected_fill_count = expected_write_head - expected_read_head;
    assert(fill_count == expected_fill_count);
}

void test_ring_buffer(void) {
    basic_test();
    threaded_test();
}
