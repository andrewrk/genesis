#ifndef PATH_HPP
#define PATH_HPP

#include "byte_buffer.hpp"
#include "list.hpp"

struct DirEntry {
    ByteBuffer name;
    bool is_dir;
    bool is_file;
    bool is_link;
    bool is_hidden;
    int64_t size;
    long mtime;
};

int path_mkdirp(ByteBuffer path);
ByteBuffer path_dirname(ByteBuffer path);
ByteBuffer path_join(ByteBuffer left, ByteBuffer right);
int path_readdir(const char *dir, List<DirEntry*> &entries);


#endif
