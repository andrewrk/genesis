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
#include "locked_queue.hpp"
#include "crc32.hpp"
#include "ordered_map_file_test.hpp"
#include "os.hpp"
#include "settings_file.hpp"
#include "project.hpp"

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

static void test_locked_queue(void) {
    LockedQueue<int> queue;

    for (int i = 0; i < 99; i += 1) {
        ok_or_panic(queue.push(i));
    }

    for (int i = 0; i < 99; i += 1) {
        int value;
        ok_or_panic(queue.shift(&value));
        assert(value == i);
    }

    LockedQueue<int> queue2;
    for (int i = 0; i < 15; i += 1) {
        ok_or_panic(queue2.push(i));
        int value;
        ok_or_panic(queue2.shift(&value));
        assert(value == i);
    }
    for (int i = 0; i < 32; i += 1) {
        ok_or_panic(queue2.push(i));
    }
    for (int i = 0; i < 32; i += 1) {
        int value;
        ok_or_panic(queue2.shift(&value));
        assert(value == i);
    }
}

static void test_crc32(void) {
    static const unsigned char crc_test_1[] = {125, 129, 239, 175, 71, 13, 235, 208, 227, 34, 211, 180, 156, 52, 192, 149, 243};
    assert(crc32(0, crc_test_1, array_length(crc_test_1)) == 0x799cf6e7);

    static const unsigned char crc_test_2[] = {181, 96, 10, 51, 76, 133, 42, 132, 181, 182, 69, 220, 225};
    assert(crc32(0, crc_test_2, array_length(crc_test_2)) == 0x5d0ffa5b);

    static const unsigned char crc_test_3[] = {246, 43};
    assert(crc32(0, crc_test_3, array_length(crc_test_3)) == 0xaf83ad84);

    static const unsigned char crc_test_4[] = {126, 165, 180, 1, 62, 171, 17, 125, 80, 79, 41, 50, 190, 97, 224, 238, 229, 125, 220, 196, 118, 200, 236, 135, 92, 9};
    assert(crc32(0, crc_test_4, array_length(crc_test_4)) == 0x84d61652);

    static const unsigned char crc_test_5[] = {191, 239, 195, 252, 50, 87, 50, 173, 86, 162, 46, 47, 199, 41, 14, 66, 120, 220, 188, 153, 170, 171, 25, 4, 46, 66, 221, 32, 170};
    assert(crc32(0, crc_test_5, array_length(crc_test_5)) == 0x16fcee02);

    static const unsigned char crc_test_6[] = {195, 176, 131, 131, 51, 9, 152, 168, 31};
    assert(crc32(0, crc_test_6, array_length(crc_test_6)) == 0x1330341d);

    static const unsigned char crc_test_7[] = {112, 15, 27, 164, 9, 154, 136, 166, 246, 67};
    assert(crc32(0, crc_test_7, array_length(crc_test_7)) == 0xbcdd7f51);

    static const unsigned char crc_test_8[] = {239, 182, 45, 7, 215, 9, 233, 77, 59, 117, 183, 226};
    assert(crc32(0, crc_test_8, array_length(crc_test_8)) == 0xb0d089cb);

    static const unsigned char crc_test_9[] = {228};
    assert(crc32(0, crc_test_9, array_length(crc_test_9)) == 0x7565c9ec);

    static const unsigned char crc_test_10[] = {34, 42, 179, 109, 162, 93, 93, 246, 163, 71, 245, 250, 208, 162, 131, 184, 80, 165, 198, 68, 96, 229};
    assert(crc32(0, crc_test_10, array_length(crc_test_10)) == 0x479f16e);
}

static void test_os_get_time(void) {
    double prev_time = os_get_time();
    for (int i = 0; i < 1000; i += 1) {
        double time = os_get_time();
        assert(time >= prev_time);
        prev_time = time;
    }
}

static void test_uint256(void) {
    {
        uint256 id = uint256::random();
        uint256 same_thing = uint256::parse(id.to_string());
        assert(id == same_thing);
    }

    {
        ByteBuffer buf;
        buf.resize(sizeof(uint256));

        uint256 id = uint256::random();
        id.write_be(buf.raw());

        uint256 same_thing = uint256::read_be(buf.raw());
        assert(id == same_thing);
    }

    {
        uint256 id = uint256::random();
        ByteBuffer str_rep_1 = id.to_string();
        ByteBuffer buf;
        buf.resize(sizeof(uint256));
        id.write_be(buf.raw());
        ByteBuffer str_rep_2 = buf.to_string();
        assert(ByteBuffer::compare(str_rep_1, str_rep_2) == 0);
    }
}

static void test_settings_file(void) {
    static const char *str_value = "this is a super\"robust\ntest, I promise\n";
    static const char *tmp_file_path = "/tmp/test_genesis_config";
    os_delete(tmp_file_path);

    SettingsFile *sf = settings_file_open(tmp_file_path);
    assert(ByteBuffer::compare(sf->user_name.encode(), "") == 0);
    sf->user_name = str_value;

    settings_file_commit(sf);
    settings_file_close(sf);

    sf = settings_file_open(tmp_file_path);

    assert(ByteBuffer::compare(sf->user_name.encode(), str_value) == 0);

    settings_file_close(sf);

    os_delete(tmp_file_path);
}

static void test_byte_buffer_to_string(void) {
    ByteBuffer buf("\x13\x17\x18\x21");
    assert(ByteBuffer::compare(buf.to_string(), "13171821") == 0);
}

static int compare_ints(int a, int b) {
    if (a > b)
        return 1;
    else if (a < b)
        return -1;
    else
        return 0;
}

static void test_list_sort(void) {
    List<int> list;
    for (int i = 0; i < 100; i += 1) {
        int x = os_random_double() * 100000;
        ok_or_panic(list.append(x));
    }
    list.sort<compare_ints>();

    for (int i = 0; i < list.length() - 1; i += 1) {
        assert(list.at(i) <= list.at(i + 1));
    }
}

static void test_basic_project_editing(void) {
    static const char *tmp_proj_path = "/tmp/test_genesis_project.gdaw";
    os_delete(tmp_proj_path);

    uint256 user_id = uint256::random();
    User *user = user_create(user_id, os_get_user_name());

    uint256 project_id = uint256::random();
    Project *project;
    ok_or_panic(project_create(tmp_proj_path, project_id, user, &project));

    assert(project->user_list.length() == 1);
    assert(project->user_list.at(0)->id == user_id);

    project_close(project);
    project = nullptr;

    int err = project_open(tmp_proj_path, user, &project);
    assert(err == 0);

    assert(project->id == project_id);
    assert(project->track_list.length() == 1);

    assert(project->user_list.length() == 1);
    assert(project->user_list.at(0)->id == user_id);

    assert(project->undo_stack.length() == 0);
    Track *last_track = project->track_list.at(project->track_list.length() - 1);
    project_insert_track(project, last_track, nullptr);

    assert(project->track_list.length() == 2);
    assert(project->undo_stack.length() == 1);

    project_undo(project);
    assert(project->track_list.length() == 1);

    project_redo(project);
    assert(project->track_list.length() == 2);

    project_undo(project);
    assert(project->track_list.length() == 1);

    project_close(project);
    project = nullptr;

    err = project_open(tmp_proj_path, user, &project);
    assert(err == 0);

    assert(project->user_list.length() == 1);
    assert(project->user_list.at(0)->id == user_id);

    assert(project->id == project_id);
    assert(project->track_list.length() == 1);

    project_redo(project);
    assert(project->track_list.length() == 2);

    project_close(project);
    os_delete(tmp_proj_path);
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
    {"LockedQueue", test_locked_queue},
    {"crc32", test_crc32},
    {"OrderedMapFile", test_ordered_map_file},
    {"os_get_time", test_os_get_time},
    {"uint256", test_uint256},
    {"SettingsFile", test_settings_file},
    {"ByteBuffer::to_string", test_byte_buffer_to_string},
    {"List::sort", test_list_sort},
    {"basic project editing", test_basic_project_editing},
    {NULL, NULL},
};

static void exec_test(struct Test *test) {
    fprintf(stderr, "testing %s...", test->name);
    test->fn();
    fprintf(stderr, "OK\n");
}

int main(int argc, char *argv[]) {
    os_init(OsRandomQualityPseudo);

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
