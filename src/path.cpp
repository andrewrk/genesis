#include "path.hpp"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

static const mode_t default_dir_mode = 0777;

int path_mkdirp(ByteBuffer path) {
    struct stat st;
    stat(path.raw(), &st);
    bool ok = S_ISDIR(st.st_mode);
    if (ok)
        return 0;

    int err = mkdir(path.raw(), default_dir_mode);
    if (!err)
        return 0;
    if (errno != ENOENT)
        return errno;

    ByteBuffer basename = path_basename(path);
    err = path_mkdirp(basename);
    if (err)
        return err;

    return path_mkdirp(path);
}

ByteBuffer path_basename(ByteBuffer path) {
    int pos = path.index_of_rev('/', path.size() - 2);
    if (pos == -1)
        panic("invalid path, separator not found");
    return path.substring(0, pos);
}

ByteBuffer path_join(ByteBuffer left, ByteBuffer right) {
    const char *fmt_str = (left.at(left.size() - 1) == '/') ? "%s%s" : "%s/%s";
    return ByteBuffer::format(fmt_str, left.raw(), right.raw());
}
