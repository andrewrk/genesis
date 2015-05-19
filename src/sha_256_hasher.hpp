#ifndef SHA_256_HASHER_HPP
#define SHA_256_HASHER_HPP

#include "byte_buffer.hpp"

struct rhash_context;

class Sha256Hasher {
public:
    Sha256Hasher();
    ~Sha256Hasher();

    void update(char *buffer, size_t len);
    void get_digest(ByteBuffer &out);
    void reset();

    rhash_context *rhc;
    bool needs_reset;
};

#endif
