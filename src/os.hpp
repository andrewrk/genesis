#ifndef OS_HPP
#define OS_HPP

#include "byte_buffer.hpp"
#include "string.hpp"

#include <stdio.h>

void os_init();

ByteBuffer os_get_home_dir(void);
ByteBuffer os_get_app_dir(void);
ByteBuffer os_get_projects_dir(void);
ByteBuffer os_get_app_config_dir(void);
ByteBuffer os_get_app_config_path(void);

uint32_t os_random_uint32(void);
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

#endif
