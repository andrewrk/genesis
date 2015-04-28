#ifndef UINT256_HPP
#define UINT256_HPP

#include "os.hpp"
#include "hash_map.hpp"

#include <inttypes.h>

template<int Size64>
class UIntOversized {
public:
    uint64_t values[Size64];

    static inline UIntOversized<Size64> zero() {
        UIntOversized<Size64> result;
        for (int i = 0; i < Size64; i += 1)
            result.values[i] = 0;
        return result;
    }

    static inline uint32_t hash(const UIntOversized<Size64> & a) {
        // it's just a bunch of xor
        uint64_t result = 0;
        for (int i = 0; i < Size64; i += 1)
            result ^= a.values[i];
        return (uint32_t)(result >> 32) ^ (uint32_t)(result & 0x00000000ffffffffULL);
    }

    static inline int compare(const UIntOversized<Size64> &a, const UIntOversized<Size64> &b) {
        for (int i = 0; i < Size64; i += 1) {
            if (a.values[i] > b.values[i])
                return 1;
            else if (a.values[i] < b.values[i])
                return -1;
        }
        return 0;
    }

    static inline UIntOversized<Size64> random() {
        UIntOversized<Size64> result;
        for (int i = 0; i < Size64; i += 1)
            result.values[i] = ((uint64_t)os_random_uint32()) << 32 | (uint64_t)os_random_uint32();
        return result;
    }

    ByteBuffer to_string() const {
        ByteBuffer result;
        for (int i = 0; i < Size64; i += 1) {
            result.append(ByteBuffer::format("%016" PRIx64, values[i]));
        }
        return result;
    }

    static inline UIntOversized<Size64> parse(const ByteBuffer &str) {
        UIntOversized<Size64> result;
        if (str.length() != Size64 * 16)
            return zero();
        for (int i = 0; i < Size64; i += 1) {
            sscanf(str.raw() + i * 16, "%016" SCNx64, &result.values[i]);
        }
        return result;
    }

    static inline UIntOversized<Size64> read_be(const char *buffer) {
        const uint8_t *buf = (const uint8_t*) buffer;
        UIntOversized<Size64> result;

        for (int i = Size64 - 1; i >= 0; i -= 1) {
            result.values[i] = 0;
            for (int byte = 0; byte < 8; byte += 1) {
                result.values[i] <<= 8;
                result.values[i] |= *buf;
                buf += 1;
            }
        }
        return result;
    }

    inline void write_be(char *buffer) const {
        uint8_t *buf = ((uint8_t*) buffer) + Size64 * 8;

        for (int i = 0; i < Size64; i += 1) {
            uint64_t x = values[i];
            for (int byte = 0; byte < 8; byte += 1) {
                buf -= 1;
                *buf = x & 0xff;
                x >>= 8;
            }
        }
    }
};

template<int Size64>
static inline bool operator==(const UIntOversized<Size64> & a, const UIntOversized<Size64> & b) {
    for (int i = 0; i < Size64; i += 1)
        if (a.values[i] != b.values[i])
            return false;
    return true;
}
template<int Size64>
static inline bool operator!=(const UIntOversized<Size64> & a, const UIntOversized<Size64> & b) {
    return !(a == b);
}

typedef UIntOversized<4> uint256;

#endif
