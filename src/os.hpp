#ifndef OS_HPP
#define OS_HPP

#include "byte_buffer.hpp"
#include "string.hpp"

void os_init();

ByteBuffer os_get_home_dir(void);
uint32_t os_random_uint32(void);
void os_open_in_browser(const String &url);
double os_get_time(void);
String os_get_user_name(void);

int os_delete(const char *path);

#endif
