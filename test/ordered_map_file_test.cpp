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

void test_ordered_map_file(void) {
    delete_tmp_file();
    test_open_close();
}
