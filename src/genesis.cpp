#include "genesis.hpp"
#include "util.hpp"
#include "path.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>

ByteBuffer genesis_dir_path;
ByteBuffer genesis_sample_path;
ByteBuffer genesis_home_dir;

static const char *get_home_dir() {
    const char *env_home_dir = getenv("HOME");
    if (env_home_dir)
        return env_home_dir;
    struct passwd *pw = getpwuid(getuid());
    return pw->pw_dir;
}

void genesis_init() {
    genesis_home_dir = ByteBuffer(get_home_dir());
    genesis_dir_path = path_join(genesis_home_dir, "genesis");
    genesis_sample_path = path_join(genesis_dir_path, "samples");
}

