#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdlib.h>

void panic(const char *format, ...) __attribute__ ((noreturn)) __attribute__ ((format (printf, 1, 2)));

// just use free to free this data
template<typename T>
static inline T *allocate(size_t count) {
    T *ptr = reinterpret_cast<T*>(malloc(count * sizeof(T)));
    if (!ptr)
        panic("allocate: out of memory");
    return ptr;
}

template<typename T>
static inline T *reallocate(T *old, size_t count) {
    T *new_ptr = reinterpret_cast<T*>(realloc(old, count * sizeof(T)));
    if (!new_ptr)
        panic("reallocate: out of memory");
    return new_ptr;
}

template<typename T>
static inline void destroy(T *ptr) {
    ptr->T::~T();
}

template<typename T>
static inline T abs(T x) {
    return (x < 0) ? -x : x;
}

template<typename T>
static inline T clamp(T min, T value, T max) {
    if (value < min) {
        return min;
    } else if (value > max) {
        return max;
    } else {
        return value;
    }
}

#endif
