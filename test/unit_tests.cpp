#undef NDEBUG

#include "byte_buffer.hpp"
#include "list.hpp"
#include "string.hpp"
#include "color.hpp"
#include "ring_buffer.hpp"
#include "error.h"
#include "thread_safe_queue_test.hpp"

#include <stdio.h>
#include <assert.h>
#include <math.h>

static void assert_no_err(int err) {
    if (err)
        panic("Error: %s", genesis_error_string(err));
}

static void debug_print_bb_list(const List<ByteBuffer> &list) {
    fprintf(stderr, "\n");
    for (long i = 0; i < list.length(); i += 1) {
        fprintf(stderr, "%zu: %s\n", i, list.at(i).raw());
    }
}

static void assert_floats_close(double a, double b) {
    double diff = fdim(a, b);
    assert(diff < 0.000001);
}

static void test_bytebuffer_split(void) {
    ByteBuffer text("abc\nxyzblahfoo\nderp");
    List<ByteBuffer> parts;
    text.split("\n", parts);

    if (parts.length() != 3) {
        debug_print_bb_list(parts);
    }
    assert(parts.length() == 3);
    assert(ByteBuffer::compare(parts.at(0), "abc") == 0);
    assert(ByteBuffer::compare(parts.at(1), "xyzblahfoo") == 0);
    assert(ByteBuffer::compare(parts.at(2), "derp") == 0);
}

static void test_string_make_lower_case(void) {
    String the_string("HELLO I LOVE CAPS LOCK");
    the_string.make_lower_case();
    assert(String::compare(the_string, "hello i love caps lock") == 0);

    String the_string2("This is the Best Sentence in the World.");
    the_string2.make_upper_case();
    assert(String::compare(the_string2, "THIS IS THE BEST SENTENCE IN THE WORLD.") == 0);
}

static void test_list_remove_range(void) {
    List<int> list;
    assert_no_err(list.append(0));
    assert_no_err(list.append(1));
    assert_no_err(list.append(2));
    assert_no_err(list.append(3));
    assert_no_err(list.append(4));
    assert_no_err(list.append(5));

    list.remove_range(2, 5);
    assert(list.length() == 3);
    assert(list.at(0) == 0);
    assert(list.at(1) == 1);
    assert(list.at(2) == 5);
}

static void test_parse_color(void) {
    glm::vec4 color = parse_color("#4D505c");
    assert_floats_close(color[0], 0.30196078431372547);
    assert_floats_close(color[1], 0.3137254901960784);
    assert_floats_close(color[2], 0.3607843137254902);
    assert_floats_close(color[3], 1.0f);
}

static void test_ring_buffer(void) {
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
    amt = sprintf(rb.write_ptr(), "writing past the end") + 1;
    rb.advance_write_ptr(amt);

    assert(rb.fill_count() == 4094 + amt);

    rb.advance_read_ptr(4094);

    assert(strcmp(rb.read_ptr(), "writing past the end") == 0);

    rb.advance_read_ptr(amt);

    assert(rb.fill_count() == 0);
    assert(rb.free_count() == rb.capacity());
}

static void test_euclidean_mod(void) {
    assert(euclidean_mod(3, 5) == 3);
    assert(euclidean_mod(8, 5) == 3);
    assert(euclidean_mod(-1, 5) == 4);
    assert(euclidean_mod(-6, 5) == 4);
}

struct Test {
    const char *name;
    void (*fn)(void);
};

static struct Test tests[] = {
    {"ByteBuffer::split", test_bytebuffer_split},
    {"String::make_lower_case", test_string_make_lower_case},
    {"List::remove_range", test_list_remove_range},
    {"parse_color", test_parse_color},
    {"RingBuffer", test_ring_buffer},
    {"euclidean_mod", test_euclidean_mod},
    {"ThreadSafeQueue", test_thread_safe_queue},
    {NULL, NULL},
};

static void exec_test(struct Test *test) {
    fprintf(stderr, "testing %s...", test->name);
    test->fn();
    fprintf(stderr, "OK\n");
}

int main(int argc, char *argv[]) {
    if (argc == 2) {
        int index = atoi(argv[1]);
        exec_test(&tests[index]);
        return 0;
    }

    struct Test *test = &tests[0];

    while (test->name) {
        exec_test(test);
        test += 1;
    }

    return 0;
}
