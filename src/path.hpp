#ifndef PATH_HPP
#define PATH_HPP

#include "byte_buffer.hpp"

int path_mkdirp(ByteBuffer path);
ByteBuffer path_basename(ByteBuffer path);
ByteBuffer path_join(ByteBuffer left, ByteBuffer right);


#endif
