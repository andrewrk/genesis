#include "os.hpp"
#include "util.hpp"
#include "random.hpp"
#include "error.h"
#include "warning.hpp"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
#define GENESIS_OS_WINDOWS

#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#if !defined(VC_EXTRALEAN)
#define VC_EXTRALEAN
#endif

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#if !defined(UNICODE)
#define UNICODE
#endif

// require Windows 7 or later
#if WINVER < 0x0601
#undef WINVER
#define WINVER 0x0601
#endif
#if _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>

#else

#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#endif

#if defined(__FreeBSD__) || defined(__MACH__)
#define GENESIS_OS_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif

#if defined(__MACH__)
#include <mach/clock.h>
#include <mach/mach.h>
#endif

struct OsThread {
#if defined(GENESIS_OS_WINDOWS)
    HANDLE handle;
    DWORD id;
#else
    pthread_attr_t attr;
    bool attr_init;

    pthread_t id;
    bool running;
#endif
    void *arg;
    void (*run)(void *arg);
};

struct OsMutex {
#if defined(GENESIS_OS_WINDOWS)
    CRITICAL_SECTION id;
#else
    pthread_mutex_t id;
    bool id_init;
#endif
};

#if defined(GENESIS_OS_KQUEUE)
static const uintptr_t notify_ident = 1;
struct OsCond {
    int kq_id;
};
#elif defined(GENESIS_OS_WINDOWS)
struct OsCond {
    CONDITION_VARIABLE id;
    CRITICAL_SECTION default_cs_id;
};
#else
struct OsCond {
    pthread_cond_t id;
    bool id_init;

    pthread_condattr_t attr;
    bool attr_init;

    pthread_mutex_t default_mutex_id;
    bool default_mutex_init;
};
#endif

#if defined(GENESIS_OS_WINDOWS)
static INIT_ONCE win32_init_once = INIT_ONCE_STATIC_INIT;
static double win32_time_resolution;
static SYSTEM_INFO win32_system_info;
#else
static bool initialized = false;
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
#if defined(__MACH__)
static clock_serv_t cclock;
#endif
#endif

static int page_size;
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
    int fd = open("/dev/urandom", O_RDONLY|O_NONBLOCK);
    if (fd == -1)
        return GenesisErrorSystemResources;

    ssize_t amt = read(fd, seed, 4);
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

    const char **argv = ok_mem(allocate_zero<const char *>(args.length() + 2));
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
    char *buf = allocate_nonzero<char>(BUFSIZ);
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



double os_get_time(void) {
#if defined(GENESIS_OS_WINDOWS)
    unsigned __int64 time;
    QueryPerformanceCounter((LARGE_INTEGER*) &time);
    return time * win32_time_resolution;
#elif defined(__MACH__)
    mach_timespec_t mts;

    kern_return_t err = clock_get_time(cclock, &mts);
    assert(!err);

    double seconds = (double)mts.tv_sec;
    seconds += ((double)mts.tv_nsec) / 1000000000.0;

    return seconds;
#else
    struct timespec tms;
    clock_gettime(CLOCK_MONOTONIC, &tms);
    double seconds = (double)tms.tv_sec;
    seconds += ((double)tms.tv_nsec) / 1000000000.0;
    return seconds;
#endif
}

#if defined(GENESIS_OS_WINDOWS)
static DWORD WINAPI run_win32_thread(LPVOID userdata) {
    struct OsThread *thread = (struct OsThread *)userdata;
    HRESULT err = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    assert(err == S_OK);
    thread->run(thread->arg);
    CoUninitialize();
    return 0;
}
#else
static void assert_no_err(int err) {
    assert(!err);
}

static void *run_pthread(void *userdata) {
    struct OsThread *thread = (struct OsThread *)userdata;
    thread->run(thread->arg);
    return NULL;
}
#endif

int os_thread_create(
        void (*run)(void *arg), void *arg,
        bool high_priority,
        struct OsThread ** out_thread)
{
    *out_thread = NULL;

    struct OsThread *thread = allocate_zero<OsThread>(1);
    if (!thread) {
        os_thread_destroy(thread);
        return GenesisErrorNoMem;
    }

    thread->run = run;
    thread->arg = arg;

#if defined(GENESIS_OS_WINDOWS)
    thread->handle = CreateThread(NULL, 0, run_win32_thread, thread, 0, &thread->id);
    if (!thread->handle) {
        os_thread_destroy(thread);
        return GenesisErrorSystemResources;
    }
    if (high_priority) {
        if (!SetThreadPriority(thread->handle, THREAD_PRIORITY_TIME_CRITICAL)) {
            emit_warning(WarningHighPriorityThread);
        }
    }
#else
    int err;
    if ((err = pthread_attr_init(&thread->attr))) {
        os_thread_destroy(thread);
        return GenesisErrorNoMem;
    }
    thread->attr_init = true;
    
    if (high_priority) {
        int max_priority = sched_get_priority_max(SCHED_FIFO);
        if (max_priority == -1) {
            os_thread_destroy(thread);
            return GenesisErrorSystemResources;
        }

        if ((err = pthread_attr_setschedpolicy(&thread->attr, SCHED_FIFO))) {
            os_thread_destroy(thread);
            return GenesisErrorSystemResources;
        }

        struct sched_param param;
        param.sched_priority = max_priority;
        if ((err = pthread_attr_setschedparam(&thread->attr, &param))) {
            os_thread_destroy(thread);
            return GenesisErrorSystemResources;
        }
    }

    if ((err = pthread_create(&thread->id, &thread->attr, run_pthread, thread))) {
        if (err == EPERM && high_priority) {
            emit_warning(WarningHighPriorityThread);
            err = pthread_create(&thread->id, NULL, run_pthread, thread);
        }
        if (err) {
            os_thread_destroy(thread);
            return GenesisErrorNoMem;
        }
    }
    thread->running = true;
#endif

    *out_thread = thread;
    return 0;
}

void os_thread_destroy(struct OsThread *thread) {
    if (!thread)
        return;

#if defined(GENESIS_OS_WINDOWS)
    if (thread->handle) {
        DWORD err = WaitForSingleObject(thread->handle, INFINITE);
        assert(err != WAIT_FAILED);
        BOOL ok = CloseHandle(thread->handle);
        assert(ok);
    }
#else

    if (thread->running) {
        assert_no_err(pthread_join(thread->id, NULL));
    }

    if (thread->attr_init) {
        assert_no_err(pthread_attr_destroy(&thread->attr));
    }
#endif

    free(thread);
}

struct OsMutex *os_mutex_create(void) {
    struct OsMutex *mutex = allocate_zero<OsMutex>(1);
    if (!mutex) {
        os_mutex_destroy(mutex);
        return NULL;
    }

#if defined(GENESIS_OS_WINDOWS)
    InitializeCriticalSection(&mutex->id);
#else
    int err;
    if ((err = pthread_mutex_init(&mutex->id, NULL))) {
        os_mutex_destroy(mutex);
        return NULL;
    }
    mutex->id_init = true;
#endif

    return mutex;
}

void os_mutex_destroy(struct OsMutex *mutex) {
    if (!mutex)
        return;

#if defined(GENESIS_OS_WINDOWS)
    DeleteCriticalSection(&mutex->id);
#else
    if (mutex->id_init) {
        assert_no_err(pthread_mutex_destroy(&mutex->id));
    }
#endif

    free(mutex);
}

void os_mutex_lock(struct OsMutex *mutex) {
#if defined(GENESIS_OS_WINDOWS)
    EnterCriticalSection(&mutex->id);
#else
    assert_no_err(pthread_mutex_lock(&mutex->id));
#endif
}

void os_mutex_unlock(struct OsMutex *mutex) {
#if defined(GENESIS_OS_WINDOWS)
    LeaveCriticalSection(&mutex->id);
#else
    assert_no_err(pthread_mutex_unlock(&mutex->id));
#endif
}

struct OsCond * os_cond_create(void) {
    struct OsCond *cond = allocate_zero<OsCond>(1);

    if (!cond) {
        os_cond_destroy(cond);
        return NULL;
    }

#if defined(GENESIS_OS_WINDOWS)
    InitializeConditionVariable(&cond->id);
    InitializeCriticalSection(&cond->default_cs_id);
#elif defined(GENESIS_OS_KQUEUE)
    cond->kq_id = kqueue();
    if (cond->kq_id == -1)
        return NULL;
#else
    if (pthread_condattr_init(&cond->attr)) {
        os_cond_destroy(cond);
        return NULL;
    }
    cond->attr_init = true;

    if (pthread_condattr_setclock(&cond->attr, CLOCK_MONOTONIC)) {
        os_cond_destroy(cond);
        return NULL;
    }

    if (pthread_cond_init(&cond->id, &cond->attr)) {
        os_cond_destroy(cond);
        return NULL;
    }
    cond->id_init = true;

    if ((pthread_mutex_init(&cond->default_mutex_id, NULL))) {
        os_cond_destroy(cond);
        return NULL;
    }
    cond->default_mutex_init = true;
#endif

    return cond;
}

void os_cond_destroy(struct OsCond *cond) {
    if (!cond)
        return;

#if defined(GENESIS_OS_WINDOWS)
    DeleteCriticalSection(&cond->default_cs_id);
#elif defined(GENESIS_OS_KQUEUE)
    close(cond->kq_id);
#else
    if (cond->id_init) {
        assert_no_err(pthread_cond_destroy(&cond->id));
    }

    if (cond->attr_init) {
        assert_no_err(pthread_condattr_destroy(&cond->attr));
    }
    if (cond->default_mutex_init) {
        assert_no_err(pthread_mutex_destroy(&cond->default_mutex_id));
    }
#endif

    free(cond);
}

void os_cond_signal(struct OsCond *cond,
        struct OsMutex *locked_mutex)
{
#if defined(GENESIS_OS_WINDOWS)
    if (locked_mutex) {
        WakeConditionVariable(&cond->id);
    } else {
        EnterCriticalSection(&cond->default_cs_id);
        WakeConditionVariable(&cond->id);
        LeaveCriticalSection(&cond->default_cs_id);
    }
#elif defined(GENESIS_OS_KQUEUE)
    struct kevent kev;
    struct timespec timeout = { 0, 0 };

    memset(&kev, 0, sizeof(kev));
    kev.ident = notify_ident;
    kev.filter = EVFILT_USER;
    kev.fflags = NOTE_TRIGGER;

    if (kevent(cond->kq_id, &kev, 1, NULL, 0, &timeout) == -1) {
        if (errno == EINTR)
            return;
        if (errno == ENOENT)
            return;
        assert(0); // kevent signal error
    }
#else
    if (locked_mutex) {
        assert_no_err(pthread_cond_signal(&cond->id));
    } else {
        assert_no_err(pthread_mutex_lock(&cond->default_mutex_id));
        assert_no_err(pthread_cond_signal(&cond->id));
        assert_no_err(pthread_mutex_unlock(&cond->default_mutex_id));
    }
#endif
}

void os_cond_timed_wait(struct OsCond *cond,
        struct OsMutex *locked_mutex, double seconds)
{
#if defined(GENESIS_OS_WINDOWS)
    CRITICAL_SECTION *target_cs;
    if (locked_mutex) {
        target_cs = &locked_mutex->id;
    } else {
        target_cs = &cond->default_cs_id;
        EnterCriticalSection(&cond->default_cs_id);
    }
    DWORD ms = seconds * 1000.0;
    SleepConditionVariableCS(&cond->id, target_cs, ms);
    if (!locked_mutex)
        LeaveCriticalSection(&cond->default_cs_id);
#elif defined(GENESIS_OS_KQUEUE)
    struct kevent kev;
    struct kevent out_kev;

    if (locked_mutex)
        assert_no_err(pthread_mutex_unlock(&locked_mutex->id));

    memset(&kev, 0, sizeof(kev));
    kev.ident = notify_ident;
    kev.filter = EVFILT_USER;
    kev.flags = EV_ADD | EV_CLEAR;

    // this time is relative
    struct timespec timeout;
    timeout.tv_nsec = (seconds * 1000000000L);
    timeout.tv_sec  = timeout.tv_nsec / 1000000000L;
    timeout.tv_nsec = timeout.tv_nsec % 1000000000L;

    if (kevent(cond->kq_id, &kev, 1, &out_kev, 1, &timeout) == -1) {
        if (errno == EINTR)
            return;
        assert(0); // kevent wait error
    }
    if (locked_mutex)
        assert_no_err(pthread_mutex_lock(&locked_mutex->id));
#else
    pthread_mutex_t *target_mutex;
    if (locked_mutex) {
        target_mutex = &locked_mutex->id;
    } else {
        target_mutex = &cond->default_mutex_id;
        assert_no_err(pthread_mutex_lock(target_mutex));
    }
    // this time is absolute
    struct timespec tms;
    clock_gettime(CLOCK_MONOTONIC, &tms);
    tms.tv_nsec += (seconds * 1000000000L);
    tms.tv_sec += tms.tv_nsec / 1000000000L;
    tms.tv_nsec = tms.tv_nsec % 1000000000L;
    int err;
    if ((err = pthread_cond_timedwait(&cond->id, target_mutex, &tms))) {
        assert(err != EPERM);
        assert(err != EINVAL);
    }
    if (!locked_mutex)
        assert_no_err(pthread_mutex_unlock(target_mutex));
#endif
}

void os_cond_wait(struct OsCond *cond,
        struct OsMutex *locked_mutex)
{
#if defined(GENESIS_OS_WINDOWS)
    CRITICAL_SECTION *target_cs;
    if (locked_mutex) {
        target_cs = &locked_mutex->id;
    } else {
        target_cs = &cond->default_cs_id;
        EnterCriticalSection(&cond->default_cs_id);
    }
    SleepConditionVariableCS(&cond->id, target_cs, INFINITE);
    if (!locked_mutex)
        LeaveCriticalSection(&cond->default_cs_id);
#elif defined(GENESIS_OS_KQUEUE)
    struct kevent kev;
    struct kevent out_kev;

    if (locked_mutex)
        assert_no_err(pthread_mutex_unlock(&locked_mutex->id));

    memset(&kev, 0, sizeof(kev));
    kev.ident = notify_ident;
    kev.filter = EVFILT_USER;
    kev.flags = EV_ADD | EV_CLEAR;

    if (kevent(cond->kq_id, &kev, 1, &out_kev, 1, NULL) == -1) {
        if (errno == EINTR)
            return;
        assert(0); // kevent wait error
    }
    if (locked_mutex)
        assert_no_err(pthread_mutex_lock(&locked_mutex->id));
#else
    pthread_mutex_t *target_mutex;
    if (locked_mutex) {
        target_mutex = &locked_mutex->id;
    } else {
        target_mutex = &cond->default_mutex_id;
        assert_no_err(pthread_mutex_lock(&cond->default_mutex_id));
    }
    int err;
    if ((err = pthread_cond_wait(&cond->id, target_mutex))) {
        assert(err != EPERM);
        assert(err != EINVAL);
    }
    if (!locked_mutex)
        assert_no_err(pthread_mutex_unlock(&cond->default_mutex_id));
#endif
}

static int internal_init(void) {
    uint32_t seed;
    int err;
    if ((err = get_random_seed(&seed)))
        return err;
    init_random_state(&random_state, seed);
#if defined(GENESIS_OS_WINDOWS)
    unsigned __int64 frequency;
    if (QueryPerformanceFrequency((LARGE_INTEGER*) &frequency)) {
        win32_time_resolution = 1.0 / (double) frequency;
    } else {
        return GenesisErrorSystemResources;
    }
    GetSystemInfo(&win32_system_info);
    page_size = win32_system_info.dwAllocationGranularity;
#else
    page_size = getpagesize();
#if defined(__MACH__)
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
#endif
#endif
    return 0;
}

int os_init(void) {
    int err;
#if defined(GENESIS_OS_WINDOWS)
    PVOID lpContext;
    BOOL pending;

    if (!InitOnceBeginInitialize(&win32_init_once, INIT_ONCE_ASYNC, &pending, &lpContext))
        return GenesisErrorSystemResources;

    if (!pending)
        return 0;

    if ((err = internal_init()))
        return err;

    if (!InitOnceComplete(&win32_init_once, INIT_ONCE_ASYNC, nullptr))
        return GenesisErrorSystemResources;
#else
    assert_no_err(pthread_mutex_lock(&init_mutex));
    if (initialized) {
        assert_no_err(pthread_mutex_unlock(&init_mutex));
        return 0;
    }
    initialized = true;
    if ((err = internal_init()))
        return err;
    assert_no_err(pthread_mutex_unlock(&init_mutex));
#endif

    return 0;
}

int os_page_size(void) {
    return page_size;
}

static inline size_t ceil_dbl_to_size_t(double x) {
    const double truncation = (size_t)x;
    return truncation + (truncation < x);
}

int os_init_mirrored_memory(struct OsMirroredMemory *mem, size_t requested_capacity) {
    size_t actual_capacity = ceil_dbl_to_size_t(requested_capacity / (double)page_size) * page_size;

#if defined(GENESIS_OS_WINDOWS)
    BOOL ok;
    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, actual_capacity * 2, NULL);
    if (!hMapFile)
        return GenesisErrorNoMem;

    for (;;) {
        // find a free address space with the correct size
        char *address = (char*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, actual_capacity * 2);
        if (!address) {
            ok = CloseHandle(hMapFile);
            assert(ok);
            return GenesisErrorNoMem;
        }

        // found a big enough address space. hopefully it will remain free
        // while we map to it. if not, we'll try again.
        ok = UnmapViewOfFile(address);
        assert(ok);

        char *addr1 = (char*)MapViewOfFileEx(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, actual_capacity, address);
        if (addr1 != address) {
            DWORD err = GetLastError();
            if (err == ERROR_INVALID_ADDRESS) {
                continue;
            } else {
                ok = CloseHandle(hMapFile);
                assert(ok);
                return GenesisErrorNoMem;
            }
        }

        char *addr2 = (char*)MapViewOfFileEx(hMapFile, FILE_MAP_WRITE, 0, 0,
                actual_capacity, address + actual_capacity);
        if (addr2 != address + actual_capacity) {
            ok = UnmapViewOfFile(addr1);
            assert(ok);

            DWORD err = GetLastError();
            if (err == ERROR_INVALID_ADDRESS) {
                continue;
            } else {
                ok = CloseHandle(hMapFile);
                assert(ok);
                return GenesisErrorNoMem;
            }
        }

        mem->priv = hMapFile;
        mem->address = address;
        break;
    }
#else
    char shm_path[] = "/dev/shm/genesis-XXXXXX";
    char tmp_path[] = "/tmp/genesis-XXXXXX";
    char *chosen_path;

    int fd = mkstemp(shm_path);
    if (fd < 0) {
        fd = mkstemp(tmp_path);
        if (fd < 0) {
            return GenesisErrorSystemResources;
        } else {
            chosen_path = tmp_path;
        }
    } else {
        chosen_path = shm_path;
    }

    if (unlink(chosen_path)) {
        close(fd);
        return GenesisErrorSystemResources;
    }

    if (ftruncate(fd, actual_capacity)) {
        close(fd);
        return GenesisErrorSystemResources;
    }

    char *address = (char*)mmap(NULL, actual_capacity * 2, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (address == MAP_FAILED)
        return GenesisErrorNoMem;

    char *other_address = (char*)mmap(address, actual_capacity, PROT_READ|PROT_WRITE,
            MAP_FIXED|MAP_SHARED, fd, 0);
    if (other_address != address) {
        munmap(address, 2 * actual_capacity);
        close(fd);
        return GenesisErrorNoMem;
    }

    other_address = (char*)mmap(address + actual_capacity, actual_capacity,
            PROT_READ|PROT_WRITE, MAP_FIXED|MAP_SHARED, fd, 0);
    if (other_address != address + actual_capacity) {
        munmap(address, 2 * actual_capacity);
        close(fd);
        return GenesisErrorNoMem;
    }

    mem->address = address;

    if (close(fd))
        return GenesisErrorSystemResources;
#endif

    mem->capacity = actual_capacity;
    return 0;
}

void os_deinit_mirrored_memory(struct OsMirroredMemory *mem) {
#if defined(GENESIS_OS_WINDOWS)
    BOOL ok;
    ok = UnmapViewOfFile(mem->address);
    assert(ok);
    ok = UnmapViewOfFile(mem->address + mem->capacity);
    assert(ok);
    ok = CloseHandle((HANDLE)mem->priv);
    assert(ok);
#else
    int err = munmap(mem->address, 2 * mem->capacity);
    assert(!err);
#endif
}

int os_concurrency(void) {
    long cpu_core_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_core_count <= 0)
        cpu_core_count = 1;
    return cpu_core_count;
}

int os_flush_file(FILE *file) {
    if (fsync(fileno(file))) {
        return GenesisErrorFileAccess;
    }
    return 0;
}
