#ifndef OS_HPP
#define OS_HPP

#include "byte_buffer.hpp"
#include "string.hpp"
#include "sha_256_hasher.hpp"

#include <stdio.h>

struct OsDirEntry {
    ByteBuffer name;
    bool is_dir;
    bool is_file;
    bool is_link;
    bool is_hidden;
    int64_t size;
    long mtime;
    int ref_count;
};

int os_init(int (*init_once)(void));

void os_get_home_dir(ByteBuffer &out);
void os_get_app_dir(ByteBuffer &out);
void os_get_projects_dir(ByteBuffer &out);
void os_get_app_config_dir(ByteBuffer &out);
void os_get_app_config_path(ByteBuffer &out);
void os_get_samples_dir(ByteBuffer &out);

uint32_t os_random_uint32(void); // 32 bits of entropy
uint64_t os_random_uint64(void); // 64 bits of entropy
double os_random_double(void); // 32 bits of entropy in range [0.0, 1.0)
void os_open_in_browser(const String &url);
double os_get_time(void);
String os_get_user_name(void);

int os_delete(const char *path);
int os_rename_clobber(const char *source, const char *dest);

struct OsTempFile {
    ByteBuffer path;
    FILE *file;
};
int os_create_temp_file(const char *dir, OsTempFile *out_tmp_file);

int os_file_flush(FILE *file);
int os_file_size(FILE *file, long *out_size);

int os_mkdirp(ByteBuffer path);
ByteBuffer os_path_dirname(ByteBuffer path);
ByteBuffer os_path_basename(const ByteBuffer &path);
void os_path_join(ByteBuffer &out, ByteBuffer left, ByteBuffer right);
ByteBuffer os_path_extension(ByteBuffer path);
void os_path_remove_extension(ByteBuffer &path);

// call unref on each entry when done
int os_copy_no_clobber(const char *source_path, const char *dest_dir,
        const char *prefix, const char *dest_extension,
        ByteBuffer &out_path, Sha256Hasher *hasher);
int os_copy(const char *source_path, const char *dest_path, Sha256Hasher *hasher);
int os_readdir(const char *dir, List<OsDirEntry*> &out_entries);
void os_dir_entry_ref(OsDirEntry *dir_entry);
void os_dir_entry_unref(OsDirEntry *dir_entry);



double os_get_time(void);

struct OsThread;
int os_thread_create(
        void (*run)(void *arg), void *arg,
        bool high_priority,
        struct OsThread ** out_thread);

void os_thread_destroy(struct OsThread *thread);


struct OsMutex;
struct OsMutex *os_mutex_create(void);
void os_mutex_destroy(struct OsMutex *mutex);
void os_mutex_lock(struct OsMutex *mutex);
void os_mutex_unlock(struct OsMutex *mutex);

struct OsCond;
struct OsCond *os_cond_create(void);
void os_cond_destroy(struct OsCond *cond);

// locked_mutex is optional. On systems that use mutexes for conditions, if you
// pass NULL, a mutex will be created and locked/unlocked for you. On systems
// that do not use mutexes for conditions, no mutex handling is necessary. If
// you already have a locked mutex available, pass it; this will be better on
// systems that use mutexes for conditions. Wakes at least one thread waiting
// on cond.
void os_cond_signal(struct OsCond *cond, struct OsMutex *locked_mutex);
// Wakes all threads waiting on cond.
void os_cond_broadcast(struct OsCond *cond, struct OsMutex *locked_mutex);
void os_cond_timed_wait(struct OsCond *cond, struct OsMutex *locked_mutex, double seconds);
void os_cond_wait(struct OsCond *cond, struct OsMutex *locked_mutex);


int os_page_size(void);

struct OsMirroredMemory {
    size_t capacity;
    char *address;
    void *priv;
};

// returned capacity might be increased from capacity to be a multiple of the
// system page size
int os_init_mirrored_memory(struct OsMirroredMemory *mem, size_t capacity);
void os_deinit_mirrored_memory(struct OsMirroredMemory *mem);

int os_concurrency(void);

struct OsMutexLocker {
    OsMutexLocker(OsMutex *mutex) {
        this->mutex = mutex;
        os_mutex_lock(mutex);
    }

    ~OsMutexLocker() {
        os_mutex_unlock(mutex);
    }

    OsMutex *mutex;
};

int os_get_current_year(void);

void os_spawn_process(const char *exe, const List<ByteBuffer> &args, bool detached);

int os_futex_wait(int *address, int val);
int os_futex_wake(int *address, int count);

#endif
