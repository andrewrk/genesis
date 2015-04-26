#ifndef CRC32_HPP
#define CRC32_HPP

#include <stdint.h>

uint32_t crc32(uint32_t crc, const unsigned char *buf, int len);

#endif
