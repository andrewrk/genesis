#include "os.hpp"
#include "genesis_editor.hpp"
#include <rhash.h>

int main(int argc, char *argv[]) {
    rhash_library_init();
    os_init(OsRandomQualityRobust);

    GenesisEditor genesis_editor;
    genesis_editor.exec();

    return 0;
}
