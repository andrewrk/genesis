#ifndef OS_HPP
#define OS_HPP

#include "byte_buffer.hpp"
#include "string.hpp"

extern ByteBuffer os_dir_path;
extern ByteBuffer os_sample_path;
extern ByteBuffer os_home_dir;

void os_init();


uint32_t os_random_uint32(void);
void os_open_in_browser(const String &url);
double os_get_time(void);
String os_get_user_name(void);

#endif
