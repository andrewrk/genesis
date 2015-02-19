#include "os.hpp"
#include "util.hpp"
#include "path.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>

ByteBuffer os_dir_path;
ByteBuffer os_sample_path;
ByteBuffer os_home_dir;

static const char *get_home_dir() {
    const char *env_home_dir = getenv("HOME");
    if (env_home_dir)
        return env_home_dir;
    struct passwd *pw = getpwuid(getuid());
    return pw->pw_dir;
}

void os_init() {
    os_home_dir = ByteBuffer(get_home_dir());
    os_dir_path = path_join(os_home_dir, "genesis");
    os_sample_path = path_join(os_dir_path, "samples");
}

