#include "util.hpp"
#include "os.hpp"
#include "genesis_editor.hpp"

static int print_usage(char *arg0) {
    fprintf(stderr, "%s [filename]\n", arg0);
    return -1;
}

int main(int argc, char *argv[]) {
    os_init();

    const char *input_filename = NULL;
    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            return print_usage(argv[0]);
        } else if (!input_filename) {
            input_filename = arg;
        } else {
            return print_usage(argv[0]);
        }
    }

    GenesisEditor genesis_editor;
    if (input_filename)
        genesis_editor.edit_file(input_filename);
    genesis_editor.exec();

    return 0;
}
