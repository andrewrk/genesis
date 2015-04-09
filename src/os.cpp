#include "os.hpp"
#include "util.hpp"
#include "path.hpp"
#include "random.hpp"
#include "error.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <pwd.h>
#include <stdlib.h>

ByteBuffer os_dir_path;
ByteBuffer os_sample_path;
ByteBuffer os_home_dir;

static RandomState random_state;

static const char *get_home_dir() {
    const char *env_home_dir = getenv("HOME");
    if (env_home_dir)
        return env_home_dir;
    struct passwd *pw = getpwuid(getuid());
    return pw->pw_dir;
}

static int get_random_seed(uint32_t *seed) {
    int fd = open("/dev/random", O_RDONLY|O_NONBLOCK);
    if (fd == -1)
        return GenesisErrorSystemResources;

    int amt = read(fd, seed, 4);
    if (amt != 4) {
        close(fd);
        return GenesisErrorSystemResources;
    }

    close(fd);
    return 0;
}

void os_init() {
    os_home_dir = ByteBuffer(get_home_dir());
    os_dir_path = path_join(os_home_dir, "genesis");
    os_sample_path = path_join(os_dir_path, "samples");

    uint32_t seed;
    int err = get_random_seed(&seed);
    if (err)
        panic("Unable to get random seed: %s", genesis_error_string(err));
    init_random_state(&random_state, seed);
}
