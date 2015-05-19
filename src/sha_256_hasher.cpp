#include "sha_256_hasher.hpp"
#include "util.hpp"

#include <rhash.h>

Sha256Hasher::Sha256Hasher() {
    rhash_library_init();
    rhc = ok_mem(rhash_init(RHASH_SHA256));
    needs_reset = false;
}

Sha256Hasher::~Sha256Hasher() {
    rhash_free(rhc);
}

void Sha256Hasher::update(char *buffer, size_t len) {
    assert(!needs_reset);
    rhash_update(rhc, buffer, len);
}

void Sha256Hasher::get_digest(ByteBuffer &out) {
    assert(!needs_reset);
    rhash_final(rhc, nullptr);
    int digest_size = rhash_get_digest_size(RHASH_SHA256);
    out.resize(digest_size);
    rhash_print(out.raw(), rhc, RHASH_SHA256, RHPR_RAW);
    needs_reset = true;
}

void Sha256Hasher::reset() {
    rhash_reset(rhc);
    needs_reset = false;
}
