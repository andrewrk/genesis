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
#include <time.h>

static RandomState random_state;

ByteBuffer os_get_home_dir(void) {
    const char *env_home_dir = getenv("HOME");
    if (env_home_dir)
        return env_home_dir;
    struct passwd *pw = getpwuid(getuid());
    return pw->pw_dir;
}

ByteBuffer os_get_app_dir(void) {
    return path_join(os_get_home_dir(), ".genesis");
}

ByteBuffer os_get_projects_dir(void) {
    return path_join(os_get_app_dir(), "projects");
}

ByteBuffer os_get_app_config_dir(void) {
    return os_get_app_dir();
}

ByteBuffer os_get_app_config_path(void) {
    return path_join(os_get_app_config_dir(), "config");
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

uint64_t os_random_uint64(void) {
    uint32_t buf[2];
    buf[0] = os_random_uint32();
    buf[1] = os_random_uint32();
    uint64_t *ptr = reinterpret_cast<uint64_t *>(buf);
    return *ptr;
}

double os_random_double(void) {
    uint32_t x = os_random_uint32();
    return ((double)x) / (((double)(UINT32_MAX))+1);
}

void os_init(OsRandomQuality random_quality) {
    uint32_t seed;
    switch (random_quality) {
    case OsRandomQualityRobust:
        {
            int err = get_random_seed(&seed);
            if (err)
                panic("Unable to get random seed: %s", genesis_error_string(err));
            break;
        }
    case OsRandomQualityPseudo:
        seed = time(nullptr) + getpid();
        break;
    }
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
    struct timespec tms;
    clock_gettime(CLOCK_MONOTONIC, &tms);
    double seconds = (double)tms.tv_sec;
    seconds += ((double)tms.tv_nsec) / 1000000000.0;
    return seconds;
}

String os_get_user_name(void) {
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    return pw ? pw->pw_name : "Unknown User";
}

int os_delete(const char *path) {
    if (unlink(path))
        return GenesisErrorFileAccess;
    return 0;
}

int os_rename_clobber(const char *source, const char *dest) {
    return rename(source, dest) ? GenesisErrorFileAccess : 0;
}

int os_create_temp_file(const char *dir, OsTempFile *out_tmp_file) {
    out_tmp_file->path = path_join(dir, "XXXXXX");
    int fd = mkstemp(out_tmp_file->path.raw());
    if (fd == -1)
        return GenesisErrorFileAccess;
    out_tmp_file->file = fdopen(fd, "w+");
    if (!out_tmp_file->file) {
        close(fd);
        return GenesisErrorNoMem;
    }
    return 0;
}
