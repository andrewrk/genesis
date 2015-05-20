#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <new>

void panic(const char *format, ...)
    __attribute__((cold))
    __attribute__ ((noreturn))
    __attribute__ ((format (printf, 1, 2)));

// create<MyClass>(a, b) is equivalent to: new MyClass(a, b)
template<typename T, typename... Args>
__attribute__((malloc)) static inline T * create(Args... args) {
    T * ptr = reinterpret_cast<T*>(malloc(sizeof(T)));
    if (!ptr)
        panic("create: out of memory");
    new (ptr) T(args...);
    return ptr;
}

// same as create, except zeroes allocated memory before calling constructor
// and returns NULL instead of panicking
template<typename T, typename... Args>
__attribute__((malloc)) static inline T * create_zero(Args... args) {
    T * ptr = reinterpret_cast<T*>(calloc(1, sizeof(T)));
    if (ptr)
        new (ptr) T(args...);
    return ptr;
}

// allocate<MyClass>(10) is equivalent to: new MyClass[10]
// calls the default constructor for each item in the array.
template<typename T>
__attribute__((malloc)) static inline T * allocate(size_t count) {
    T * ptr = reinterpret_cast<T*>(malloc(count * sizeof(T)));
    if (!ptr)
        panic("allocate: out of memory");
    for (size_t i = 0; i < count; i++)
        new (&ptr[i]) T;
    return ptr;
}

// allocate zeroed memory and return NULL instead of panicking.
template<typename T>
__attribute__((malloc)) static inline T *allocate_zero(size_t count) {
    return reinterpret_cast<T*>(calloc(count, sizeof(T)));
    T * ptr = reinterpret_cast<T*>(calloc(count, sizeof(T)));
    if (ptr) {
        for (size_t i = 0; i < count; i++)
            new (&ptr[i]) T;
    }
    return ptr;
}

// allocate memory and return NULL instead of panicking. do not run constructors.
template<typename T>
__attribute__((malloc)) static inline T *allocate_safe(size_t count) {
    return reinterpret_cast<T*>(malloc(count * sizeof(T)));
}

// Pass in a pointer to an array of old_count items.
// You will get a pointer to an array of new_count items
// where the first old_count items will have the same bits as the array you
// passed in.
// Calls the default constructor on all the new elements.
// The returned pointer may not be equal to the given pointer,
// and no methods are called in the event of copying/moving the bits to a new
// buffer (no default constructor, no assignment operator, and no destructor,
// as you would expect from a manual implementation). This means you can't
// count on any pointers to these elements remaining valid after this call.
// If new_count is less than old_count, i.e. this is a shrinking operation,
// behavior is undefined.
template<typename T>
static inline T * reallocate(T * old, size_t old_count, size_t new_count) {
    assert(old_count <= new_count);
    T * new_ptr = reinterpret_cast<T*>(realloc(old, new_count * sizeof(T)));
    if (!new_ptr)
        panic("reallocate: out of memory");
    for (size_t i = old_count; i < new_count; i += 1)
        new (&new_ptr[i]) T;
    return new_ptr;
}

// return NULL instead of panicking
template<typename T>
static inline T * reallocate_safe(T * old, size_t old_count, size_t new_count) {
    assert(old_count <= new_count);
    T * new_ptr = reinterpret_cast<T*>(realloc(old, new_count * sizeof(T)));
    if (new_ptr) {
        for (size_t i = old_count; i < new_count; i += 1)
            new (&new_ptr[i]) T;
    }
    return new_ptr;
}

// calls destructors and frees the memory.
// the count parameter is only used to call destructors of array elements.
// provide a count of 1 if this is not an array,
// or a count of 0 to skip the destructors.
template<typename T>
static inline void destroy(T * ptr, size_t count) {
    if (ptr) {
        for (size_t i = 0; i < count; i += 1)
            ptr[i].~T();
    }
    free(ptr);
}

template <typename T, long n>
constexpr long array_length(const T (&)[n]) {
    return n;
}

static inline float abs(float x) {
    return fabsf(x);
}

static inline double abs(double x) {
    return fabs(x);
}

template<typename T>
static inline T abs(T x) {
    return (x < 0) ? -x : x;
}

static inline float min(float a, float b) {
    return fminf(a, b);
}

static inline double min(double a, double b) {
    return fmin(a, b);
}

template <typename T>
static inline T min(T a, T b) {
    return (a <= b) ? a : b;
}

static inline float max(float a, float b) {
    return fmaxf(a, b);
}

static inline double max(double a, double b) {
    return fmax(a, b);
}

template <typename T>
static inline T max(T a, T b) {
    return (a >= b) ? a : b;
}

template<typename T>
static inline T clamp(T min_value, T value, T max_value) {
    return max(min(value, max_value), min_value);
}

static inline float abs_diff(float a, float b) {
    return fdimf(a, b);
}

static inline double abs_diff(double a, double b) {
    return fdim(a, b);
}

template<typename T>
static inline T abs_diff(T a, T b) {
    return abs(a - b);
}

template <typename T>
static inline T euclidean_mod(T a, T base) {
    return (a < 0) ? ((a % base + base) % base) : (a % base);
}

template <typename T>
static inline T sign(T x) {
    if (x > 0)
        return 1;
    else if (x < 0)
        return -1;
    else
        return 0;
}

template<typename T, int(*Comparator)(T, T)>
void quick_sort(T *in_place_list, int size) {
    auto qsort_comparator = [](const void *void_a, const void *void_b) -> int {
        T *a = (T *)void_a;
        T *b = (T *)void_b;
        return Comparator(*a, *b);
    };
    qsort(in_place_list, size, sizeof(T), qsort_comparator);
}

static inline void write_uint32be(void *buffer, uint32_t x) {
    uint8_t *buf = (uint8_t*) buffer;

    buf[3] = x & 0xff;

    x >>= 8;
    buf[2] = x & 0xff;

    x >>= 8;
    buf[1] = x & 0xff;

    x >>= 8;
    buf[0] = x & 0xff;
}

static inline void write_uint64be(void *buffer, uint32_t x) {
    uint8_t *buf = (uint8_t*) buffer;

    buf[7] = x & 0xff;

    x >>= 8;
    buf[6] = x & 0xff;

    x >>= 8;
    buf[5] = x & 0xff;

    x >>= 8;
    buf[4] = x & 0xff;

    x >>= 8;
    buf[3] = x & 0xff;

    x >>= 8;
    buf[2] = x & 0xff;

    x >>= 8;
    buf[1] = x & 0xff;

    x >>= 8;
    buf[0] = x & 0xff;
}

static inline uint32_t read_uint32be(const void *buffer) {
    uint8_t *buf = (uint8_t*) buffer;
    uint32_t result = buf[0];

    result <<= 8;
    result |= buf[1];

    result <<= 8;
    result |= buf[2];

    result <<= 8;
    result |= buf[3];

    return result;
}

static inline uint64_t read_uint64be(const void *buffer) {
    uint8_t *buf = (uint8_t*) buffer;
    uint64_t result = buf[0];

    result <<= 8;
    result |= buf[1];

    result <<= 8;
    result |= buf[2];

    result <<= 8;
    result |= buf[3];

    result <<= 8;
    result |= buf[4];

    result <<= 8;
    result |= buf[5];

    result <<= 8;
    result |= buf[6];

    result <<= 8;
    result |= buf[7];

    return result;
}

char * create_formatted_str(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

unsigned int greatest_common_denominator(unsigned int u, unsigned int v);

#endif
