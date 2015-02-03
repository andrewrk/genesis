#include "util.hpp"
#include "path.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>

static ByteBuffer genesis_dir_path;
static ByteBuffer genesis_sample_path;
static const char *home_dir;

static const char *get_home_dir() {
    const char *env_home_dir = getenv("HOME");
    if (env_home_dir)
        return env_home_dir;
    struct passwd *pw = getpwuid(getuid());
    return pw->pw_dir;
}

void genesis_init() {
    home_dir = get_home_dir();
    genesis_dir_path = path_join(home_dir, "genesis");
    genesis_sample_path = path_join(genesis_dir_path, "samples");
    int err = path_mkdirp(genesis_sample_path);
    if (err)
        panic("unable to make genesis sample path");
}

