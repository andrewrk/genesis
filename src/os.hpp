#ifndef OS_HPP
#define OS_HPP

#include "byte_buffer.hpp"
#include "string.hpp"

#include <stdio.h>

enum OsRandomQuality {
    OsRandomQualityRobust,
    OsRandomQualityPseudo,
};

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

void os_init(OsRandomQuality random_quality);

ByteBuffer os_get_home_dir(void);
ByteBuffer os_get_app_dir(void);
ByteBuffer os_get_projects_dir(void);
ByteBuffer os_get_app_config_dir(void);
ByteBuffer os_get_app_config_path(void);
ByteBuffer os_get_samples_dir(void);

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

int os_mkdirp(ByteBuffer path);
ByteBuffer os_path_dirname(ByteBuffer path);
ByteBuffer os_path_join(ByteBuffer left, ByteBuffer right);
ByteBuffer os_path_extension(ByteBuffer path);

// call unref on each entry when done
int os_copy_no_clobber(const char *source_path, const char *dest_dir,
        const char *dest_extension, ByteBuffer &out_path);
int os_copy(const char *source_path, const char *dest_path);
int os_readdir(const char *dir, List<OsDirEntry*> &out_entries);
void os_dir_entry_ref(OsDirEntry *dir_entry);
void os_dir_entry_unref(OsDirEntry *dir_entry);

#endif
