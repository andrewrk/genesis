#include "os.hpp"
#include "genesis_editor.hpp"

int main(int argc, char *argv[]) {
    os_init();

    GenesisEditor genesis_editor;
    genesis_editor.exec();

    return 0;
}
