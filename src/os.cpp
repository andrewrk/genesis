#include "os.hpp"
#include "util.hpp"
#include "random.hpp"
#include "error.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

static RandomState random_state;
static const mode_t default_dir_mode = 0777;

ByteBuffer os_get_home_dir(void) {
    const char *env_home_dir = getenv("HOME");
    if (env_home_dir)
        return env_home_dir;
    struct passwd *pw = getpwuid(getuid());
    return pw->pw_dir;
}

ByteBuffer os_get_app_dir(void) {
    return os_path_join(os_get_home_dir(), ".genesis");
}

ByteBuffer os_get_projects_dir(void) {
    return os_path_join(os_get_app_dir(), "projects");
}

ByteBuffer os_get_samples_dir(void) {
    return os_path_join(os_get_app_dir(), "samples");
}

ByteBuffer os_get_app_config_dir(void) {
    return os_get_app_dir();
}

ByteBuffer os_get_app_config_path(void) {
    return os_path_join(os_get_app_config_dir(), "config");
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

static uint32_t get_seed(OsRandomQuality random_quality) {
    switch (random_quality) {
    case OsRandomQualityRobust:
        {
            uint32_t result;
            int err;
            if ((err = get_random_seed(&result)))
                panic("Unable to get random seed: %s", genesis_error_string(err));
            return result;
        }
    case OsRandomQualityPseudo:
        return (time(nullptr) + getpid());
    }
    panic("invalid random quality enum value");
}

void os_init(OsRandomQuality random_quality) {
    uint32_t seed = get_seed(random_quality);
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
    ok_or_panic(args.append(url.encode()));
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
    out_tmp_file->path = os_path_join(dir, "XXXXXX");
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

int os_mkdirp(ByteBuffer path) {
    struct stat st;
    int err = stat(path.raw(), &st);
    if (!err && S_ISDIR(st.st_mode))
        return 0;

    err = mkdir(path.raw(), default_dir_mode);
    if (!err)
        return 0;
    if (errno != ENOENT)
        return GenesisErrorFileAccess;

    ByteBuffer dirname = os_path_dirname(path);
    err = os_mkdirp(dirname);
    if (err)
        return err;

    return os_mkdirp(path);
}

ByteBuffer os_path_dirname(ByteBuffer path) {
    ByteBuffer result;
    const char *ptr = path.raw();
    const char *last_slash = NULL;
    while (*ptr) {
        const char *next = ptr + 1;
        if (*ptr == '/' && *next)
            last_slash = ptr;
        ptr = next;
    }
    if (!last_slash) {
        if (path.at(0) == '/')
            result.append("/", 1);
        return result;
    }
    ptr = path.raw();
    while (ptr != last_slash) {
        result.append(ptr, 1);
        ptr += 1;
    }
    if (result.length() == 0 && path.at(0) == '/')
        result.append("/", 1);
    return result;
}

ByteBuffer os_path_basename(const ByteBuffer &path) {
    int last_slash = path.index_of_rev('/');
    assert(last_slash >= 0);

    int start_index = last_slash + 1;
    if (start_index < path.length())
        return path.substring(start_index, path.length());
    else
        return "";
}

ByteBuffer os_path_join(ByteBuffer left, ByteBuffer right) {
    const char *fmt_str = (left.at(left.length() - 1) == '/') ? "%s%s" : "%s/%s";
    return ByteBuffer::format(fmt_str, left.raw(), right.raw());
}

ByteBuffer os_path_extension(ByteBuffer path) {
    int dot_index = path.index_of_rev('.');
    if (dot_index == -1)
        return "";

    return path.substring(dot_index, path.length());
}

void os_path_remove_extension(ByteBuffer &path) {
    int dot_index = path.index_of_rev('.');
    if (dot_index == -1)
        return;

    path.resize(dot_index);
}

int os_readdir(const char *dir, List<OsDirEntry*> &entries) {
    for (int i = 0; i < entries.length(); i += 1)
        os_dir_entry_unref(entries.at(i));
    entries.clear();

    DIR *dp = opendir(dir);
    if (!dp) {
        switch (errno) {
            case EACCES:
                return GenesisErrorPermissionDenied;
            case EMFILE: // fall through
            case ENFILE:
                return GenesisErrorSystemResources;
            case ENOENT:
                return GenesisErrorFileNotFound;
            case ENOMEM:
                return GenesisErrorNoMem;
            case ENOTDIR:
                return GenesisErrorNotDir;
            default:
                panic("unexpected error from opendir: %s", strerror(errno));
        }
    }
    struct dirent *ep;
    while ((ep = readdir(dp))) {
        if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
            continue;
        ByteBuffer full_path = os_path_join(dir, ep->d_name);
        struct stat st;
        if (stat(full_path.raw(), &st)) {
            int c_err = errno;
            switch (c_err) {
                case EACCES:
                    closedir(dp);
                    return GenesisErrorPermissionDenied;
                case ELOOP:
                case ENAMETOOLONG:
                case EOVERFLOW:
                    closedir(dp);
                    return GenesisErrorUnimplemented;
                case ENOENT:
                case ENOTDIR:
                    // we can simply skip this entry
                    continue;
                case ENOMEM:
                    closedir(dp);
                    return GenesisErrorNoMem;
                default:
                    panic("unexpected error from stat: %s", strerror(errno));
            }
        }
        OsDirEntry *entry = create_zero<OsDirEntry>();
        if (!entry) {
            closedir(dp);
            return GenesisErrorNoMem;
        }
        entry->name = ByteBuffer(ep->d_name);
        entry->is_dir = S_ISDIR(st.st_mode);
        entry->is_file = S_ISREG(st.st_mode);
        entry->is_link = S_ISLNK(st.st_mode);
        entry->is_hidden = ep->d_name[0] == '.';
        entry->size = st.st_size;
        entry->mtime = st.st_mtime;
        entry->ref_count = 1;

        if (entries.append(entry)) {
            closedir(dp);
            os_dir_entry_unref(entry);
            return GenesisErrorNoMem;
        }
    }
    closedir(dp);
    return 0;
}

void os_dir_entry_ref(OsDirEntry *dir_entry) {
    dir_entry->ref_count += 1;
}

void os_dir_entry_unref(OsDirEntry *dir_entry) {
    if (dir_entry) {
        dir_entry->ref_count -= 1;
        assert(dir_entry->ref_count >= 0);
        if (dir_entry->ref_count == 0)
            destroy(dir_entry, 1);
    }
}

static int copy_open_files(FILE *in_f, FILE *out_f, Sha256Hasher *hasher) {
    char *buf = allocate_safe<char>(BUFSIZ);
    if (!buf)
        return GenesisErrorNoMem;

    for (;;) {
        size_t amt_read = fread(buf, 1, BUFSIZ, in_f);
        size_t amt_written = fwrite(buf, 1, amt_read, out_f);
        if (hasher)
            hasher->update(buf, amt_read);
        if (amt_read != BUFSIZ) {
            if (feof(in_f)) {
                destroy(buf, 1);
                if (amt_written != amt_read)
                    return GenesisErrorFileAccess;
                else
                    return 0;
            } else {
                destroy(buf, 1);
                return GenesisErrorFileAccess;
            }
        } else if (amt_written != amt_read) {
            destroy(buf, 1);
            return GenesisErrorFileAccess;
        }
    }
}

int os_copy_no_clobber(const char *source_path, const char *dest_dir,
        const char *prefix, const char *dest_extension,
        ByteBuffer &out_path, Sha256Hasher *hasher)
{
    ByteBuffer dir_plus_prefix = os_path_join(dest_dir, prefix);
    ByteBuffer full_path;
    int out_fd;
    for (int counter = 0;; counter += 1) {
        full_path = dir_plus_prefix;
        if (counter != 0)
            full_path.append(ByteBuffer::format("%d", counter));
        full_path.append(dest_extension);
        out_fd = open(full_path.raw(), O_CREAT|O_WRONLY|O_EXCL, 0660);
        if (out_fd == -1) {
            if (errno == EEXIST) {
                continue;
            } else if (errno == ENOMEM) {
                return GenesisErrorNoMem;
            } else {
                return GenesisErrorFileAccess;
            }
        }
        break;
    }

    FILE *out_f = fdopen(out_fd, "wb");
    if (!out_f) {
        close(out_fd);
        os_delete(full_path.raw());
        return GenesisErrorNoMem;
    }

    FILE *in_f = fopen(source_path, "rb");
    if (!in_f) {
        fclose(out_f);
        os_delete(full_path.raw());
        return GenesisErrorFileAccess;
    }

    int err;
    if ((err = copy_open_files(in_f, out_f, hasher))) {
        fclose(in_f);
        fclose(out_f);
        os_delete(full_path.raw());
        return err;
    }
    out_path = full_path;

    return 0;
}

int os_copy(const char *source_path, const char *dest_path, Sha256Hasher *hasher) {
    FILE *in_f = fopen(source_path, "rb");
    if (!in_f)
        return GenesisErrorFileAccess;

    FILE *out_f = fopen(dest_path, "wb");
    if (!out_f) {
        fclose(in_f);
        return GenesisErrorFileAccess;
    }

    int err;
    if ((err = copy_open_files(in_f, out_f, hasher))) {
        fclose(in_f);
        fclose(out_f);
        os_delete(dest_path);
        return err;
    }

    return 0;
}
