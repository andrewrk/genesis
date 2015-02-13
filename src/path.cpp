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

    ByteBuffer dirname = path_dirname(path);
    err = path_mkdirp(dirname);
    if (err)
        return err;

    return path_mkdirp(path);
}

ByteBuffer path_dirname(ByteBuffer path) {
    ByteBuffer result;
    const char *ptr = path.raw();
    const char *last_slash = NULL;
    while (*ptr) {
        const char *next = ptr + 1;
        if (*ptr == '/' && *next)
            last_slash = ptr;
        ptr = next;
    }
    if (!last_slash) {
        if (path.at(0) == '/')
            result.append("/", 1);
        return result;
    }
    ptr = path.raw();
    while (ptr != last_slash) {
        result.append(ptr, 1);
        ptr += 1;
    }
    if (result.length() == 0 && path.at(0) == '/')
        result.append("/", 1);
    return result;
}

ByteBuffer path_join(ByteBuffer left, ByteBuffer right) {
    const char *fmt_str = (left.at(left.length() - 1) == '/') ? "%s%s" : "%s/%s";
    return ByteBuffer::format(fmt_str, left.raw(), right.raw());
}
