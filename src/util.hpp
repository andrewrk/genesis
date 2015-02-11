#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdlib.h>
#include <new>

void panic(const char *format, ...) __attribute__ ((noreturn)) __attribute__ ((format (printf, 1, 2)));

// It's safe to use stdlib free() to free the ptr returned by allocate().
// however, since allocated memory created with create() must be freed with
// destroy(), you might consider always using destroy() to free memory for
// consistency's sake. I have verified that calling the constructor and
// destructor of a primitive type does nothing, and generated code using
// create()/destroy() for primitive types is identical to generated code using 
// allocate()/free()
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

// allocates enough memory for T and then calls the constructor of T.
// use destroy() to free the memory when done.
template<typename T, typename... Args>
static inline T *create(Args... args) {
    T *ptr = allocate<T>(1);
    new (ptr) T(args...);
    return ptr;
}

// calls the destructor of T and then frees the memory
template<typename T>
static inline void destroy(T *ptr) {
    ptr->T::~T();
    free(ptr);
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

template <typename T, size_t n>
constexpr size_t array_length(const T (&)[n]) {
    return n;
}

#endif
