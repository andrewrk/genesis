#include "os.hpp"
#include "util.hpp"
#include "path.hpp"
#include "random.hpp"
#include "error.h"
#include "glfw.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <errno.h>

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

uint32_t os_random_uint32(void) {
    return get_random(&random_state);
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

void os_spawn_process(const char *exe, const List<ByteBuffer> &args, bool detached) {
    pid_t pid = fork();
    if (pid == -1)
        panic("fork failed");
    if (pid != 0)
        return;
    if (detached) {
        if (setsid() == -1)
            panic("process detach failed");
    }

    const char **argv = allocate<const char *>(args.length() + 2);
    argv[0] = exe;
    argv[args.length() + 1] = nullptr;
    for (int i = 0; i < args.length(); i += 1) {
        argv[i + 1] = args.at(i).raw();
    }
    execvp(exe, const_cast<char * const *>(argv));
    panic("execvp failed: %s", strerror(errno));
}

void os_open_in_browser(const String &url) {
    List<ByteBuffer> args;
    if (args.append(url.encode()))
        panic("out of memory");
    os_spawn_process("xdg-open", args, true);
}

double os_get_time(void) {
    return glfwGetTime();
}

String os_get_user_name(void) {
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    return pw ? pw->pw_name : "Unknown User";
}
