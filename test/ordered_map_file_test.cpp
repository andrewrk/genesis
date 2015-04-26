#include "ordered_map_file_test.hpp"
#include "ordered_map_file.hpp"
#include "os.hpp"

static const char *tmp_file_path = "/tmp/genesis_test.gdaw";

static void delete_tmp_file(void) {
    os_delete(tmp_file_path);
}

static void test_open_close(void) {
    OrderedMapFile *omf;
    int err = ordered_map_file_open(tmp_file_path, &omf);
    assert(err == 0);
    assert(omf);
    ordered_map_file_done_reading(omf);
    ordered_map_file_close(omf);
    delete_tmp_file();
}

static void test_bogus_file(void) {
    FILE *f = fopen(tmp_file_path, "wb");
    fprintf(f, "aoeuaouaeouaouaoeu");
    fclose(f);

    OrderedMapFile *omf = nullptr;
    int err = ordered_map_file_open(tmp_file_path, &omf);
    assert(err == GenesisErrorInvalidFormat);
    assert(!omf);

    delete_tmp_file();
}

static void test_simple_data(void) {
    OrderedMapFile *omf;
    int err = ordered_map_file_open(tmp_file_path, &omf);
    assert(err == 0);
    assert(omf);
    ordered_map_file_done_reading(omf);

    OrderedMapFileBatch *batch = ordered_map_file_batch_create(omf);

    OrderedMapFileBuffer *key = ordered_map_file_buffer_create(1);
    OrderedMapFileBuffer *value = ordered_map_file_buffer_create(9);

    key->data[0] = 91;
    strcpy(value->data, "aoeuasdf");

    err = ordered_map_file_batch_exec(batch);

    ordered_map_file_close(omf);
    omf = nullptr;

    err = ordered_map_file_open(tmp_file_path, &omf);
    assert(err == 0);
    assert(omf);

    // TODO read the key and make sure the data is intact

    ordered_map_file_done_reading(omf);
    ordered_map_file_close(omf);
    delete_tmp_file();
}

void test_ordered_map_file(void) {
    delete_tmp_file();
    test_open_close();
    test_bogus_file();
    test_simple_data();
}
