#ifndef UINT256_HPP
#define UINT256_HPP

#include "os.hpp"
#include "hash_map.hpp"

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
