#undef NDEBUG

#include "byte_buffer.hpp"
#include "list.hpp"
#include "string.hpp"

#include <stdio.h>
#include <assert.h>

static void debug_print_bb_list(const List<ByteBuffer> &list) {
    fprintf(stderr, "\n");
    for (int i = 0; i < list.length(); i += 1) {
        fprintf(stderr, "%d: %s\n", i, list.at(i).raw());
    }
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

struct Test {
    const char *name;
    void (*fn)(void);
};

static struct Test tests[] = {
    {"ByteBuffer::split", test_bytebuffer_split},
    {"String::make_lower_case", test_string_make_lower_case},
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
