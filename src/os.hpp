#ifndef OS_HPP
#define OS_HPP

#include "byte_buffer.hpp"

extern ByteBuffer os_dir_path;
extern ByteBuffer os_sample_path;
extern ByteBuffer os_home_dir;

void os_init();


uint32_t random_uint32(void);

#endif
