#include "path.hpp"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

static const mode_t default_dir_mode = 0777;

int path_mkdirp(ByteBuffer path) {
    struct stat st;
    int err = stat(path.raw(), &st);
    if (!err && S_ISDIR(st.st_mode))
        return 0;

    err = mkdir(path.raw(), default_dir_mode);
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

int path_readdir(const char *dir, List<DirEntry*> &entries) {
    DIR *dp = opendir(dir);
    if (!dp)
        return errno;
    struct dirent *ep;
    while ((ep = readdir(dp))) {
        if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
            continue;
        ByteBuffer full_path = path_join(dir, ep->d_name);
        struct stat st;
        if (stat(full_path.raw(), &st)) {
            int err = errno;
            closedir(dp);
            return err;
        }
        DirEntry *entry = create<DirEntry>();
        entry->name = ByteBuffer(ep->d_name);
        entry->is_dir = S_ISDIR(st.st_mode);
        entry->is_file = S_ISREG(st.st_mode);
        entry->is_link = S_ISLNK(st.st_mode);
        entry->is_hidden = ep->d_name[0] == '.';
        entry->size = st.st_size;
        entry->mtime = st.st_mtime;
        entries.append(entry);
    }
    if (closedir(dp))
        return errno;

    return 0;
}
