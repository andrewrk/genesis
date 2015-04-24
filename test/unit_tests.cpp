#undef NDEBUG

#include "byte_buffer.hpp"
#include "list.hpp"
#include "string.hpp"
#include "color.hpp"
#include "ring_buffer.hpp"
#include "error.h"
#include "thread_safe_queue_test.hpp"
#include "sort_key.hpp"
#include "debug.hpp"

#include <stdio.h>
#include <assert.h>
#include <math.h>

static void debug_print_bb_list(const List<ByteBuffer> &list) {
    fprintf(stderr, "\n");
    for (int i = 0; i < list.length(); i += 1) {
        fprintf(stderr, "%d: %s\n", i, list.at(i).raw());
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
    for (int i = 0; i < 6; i += 1)
        ok_or_panic(list.append(i));

    list.remove_range(2, 5);
    assert(list.length() == 3);
    assert(list.at(0) == 0);
    assert(list.at(1) == 1);
    assert(list.at(2) == 5);

    list.clear();
    for (int i = 0; i < 10; i += 1)
        ok_or_panic(list.append(i));

    list.remove_range(2, 3);
    assert(list.length() == 9);
    assert(list.at(1) == 1);
    assert(list.at(2) == 3);
    assert(list.at(3) == 4);
    assert(list.at(4) == 5);
    assert(list.at(8) == 9);
}

static void test_list_insert_space(void) {
    List<int> list;
    for (int i = 0; i < 25; i += 1) {
        ok_or_panic(list.append(i));
    }

    ok_or_panic(list.insert_space(0, 3));

    assert(list.length() == 28);
    assert(list.at(3) == 0);
    assert(list.at(4) == 1);
    assert(list.at(5) == 2);
    assert(list.at(27) == 24);
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
    rb.advance_read_ptr(4094);
    amt = sprintf(rb.write_ptr(), "writing past the end") + 1;
    rb.advance_write_ptr(amt);

    assert(rb.fill_count() == amt);

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

static void test_gcd(void) {
    assert(greatest_common_denominator(42, 56) == 14);
    assert(greatest_common_denominator(54, 24) == 6);
    assert(greatest_common_denominator(48000, 44100) == 300);
    assert(greatest_common_denominator(44100, 96000) == 300);
}

static void test_sort_keys_basic(void) {
    SortKey b = SortKey::single(nullptr, nullptr);
    SortKey d = SortKey::single(&b, nullptr);
    assert(SortKey::compare(b, d) < 0); // forwards
    SortKey c = SortKey::single(&b, &d);
    assert(SortKey::compare(b, c) < 0); // between
    assert(SortKey::compare(c, d) < 0);
    SortKey a = SortKey::single(nullptr, &b);
    assert(SortKey::compare(a, b) < 0); // backwards
}

static void sort_keys_count_test_size(int size, const SortKey *low, const SortKey *high) {
    List<SortKey> array;
    SortKey::multi(array, low, high, size);
    assert(array.length() == size);
    const SortKey *prev = low;
    for (int i = 0; i < array.length(); i += 1) {
        const SortKey *this_one = &array.at(i);
        if (prev)
            assert(SortKey::compare(*prev, *this_one) < 0);
        prev = this_one;
    }
    if (prev && high)
        assert(SortKey::compare(*prev, *high));
}

static void run_sort_keys_count_test(const SortKey *low, const SortKey *high) {
    sort_keys_count_test_size(0, low, high);
    sort_keys_count_test_size(1, low, high);
    sort_keys_count_test_size(2, low, high);
    sort_keys_count_test_size(3, low, high);
    sort_keys_count_test_size(1000, low, high);
}

static void test_sort_keys_count(void) {
    SortKey some_value = SortKey::single(nullptr, nullptr);
    run_sort_keys_count_test(nullptr, nullptr);
    run_sort_keys_count_test(&some_value, nullptr);
    run_sort_keys_count_test(nullptr, &some_value);
    SortKey upper = SortKey::single(&some_value, nullptr);
    run_sort_keys_count_test(&some_value, &upper);
}

struct Test {
    const char *name;
    void (*fn)(void);
};

static struct Test tests[] = {
    {"ByteBuffer::split", test_bytebuffer_split},
    {"String::make_lower_case", test_string_make_lower_case},
    {"List::remove_range", test_list_remove_range},
    {"List::insert_space", test_list_insert_space},
    {"parse_color", test_parse_color},
    {"RingBuffer", test_ring_buffer},
    {"euclidean_mod", test_euclidean_mod},
    {"ThreadSafeQueue", test_thread_safe_queue},
    {"greatest_common_denominator", test_gcd},
    {"sort keys basic", test_sort_keys_basic},
    {"sort keys count", test_sort_keys_count},
    {NULL, NULL},
};

static void exec_test(struct Test *test) {
    fprintf(stderr, "testing %s...", test->name);
    test->fn();
    fprintf(stderr, "OK\n");
}

int main(int argc, char *argv[]) {
    const char *match = nullptr;

    if (argc == 2)
        match = argv[1];

    struct Test *test = &tests[0];

    while (test->name) {
        if (!match || strstr(test->name, match))
            exec_test(test);
        test += 1;
    }

    return 0;
}
