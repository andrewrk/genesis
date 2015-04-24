#ifndef BYTE_BUFFER_HPP
#define BYTE_BUFFER_HPP

#include "list.hpp"
#include "debug.hpp"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

class ByteBuffer {
public:
    ByteBuffer();
    ByteBuffer(const ByteBuffer & copy);
    ByteBuffer(const char * str);
    ByteBuffer(const char * str, int length);
    ~ByteBuffer() {}
    ByteBuffer& operator= (const ByteBuffer& other);

    void clear() {
        _buffer.clear();
        ok_or_panic(_buffer.append(0));
    }

    const char & at(int index) const {
        return _buffer.at(index);
    }
    char & at(int index) {
        return _buffer.at(index);
    }

    static ByteBuffer format(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

    int length() const {
        return _buffer.length() - 1;
    }
    void resize(int new_length) {
        if (_buffer.resize(new_length + 1))
            panic("out of memory");
        _buffer.at(length()) = 0;
    }
    void append(const ByteBuffer &other);
    void append(const char *str);
    void append(const char *str, int length);

    void append_uint32le(uint32_t value) {
        // 00000000 00000000 00000000 xxxxxxxx
        append_uint8(value & 0xff);
        // 00000000 00000000 xxxxxxxx 00000000
        append_uint8((value >> 8) & 0xff);
        // 00000000 xxxxxxxx 00000000 00000000
        append_uint8((value >> 16) & 0xff);
        // xxxxxxxx 00000000 00000000 00000000
        append_uint8((value >> 24) & 0xff);
    }
    void append_uint16le(uint16_t value) {
        // 00000000 xxxxxxxx
        append_uint8(value & 0xff);
        // xxxxxxxx 00000000
        append_uint8((value >> 8) & 0xff);
    }
    void append_uint8(uint8_t value) {
        _buffer.at(length()) = value;
        if (_buffer.append(0))
            panic("out of memory");
    }
    uint32_t read_uint32le(int index) const {
        if (index < 0 || index + 4 > length())
            panic("bounds check");
        char *buf = _buffer.raw() + index;
        return  ((uint32_t)buf[0])        |
               (((uint32_t)buf[1]) <<  8) |
               (((uint32_t)buf[2]) << 16) |
               (((uint32_t)buf[3]) << 24) ;
    }
    uint8_t read_uint8(int index) const {
        if (index < 0 || index >= length())
            panic("bounds check");
        char *buf = _buffer.raw() + index;
        return (uint8_t)*buf;
    }
    void append_fill(int count, char value) {
        if (count < 0)
            panic("bounds check");
        int index = length();
        resize(index + count);
        char *buf = _buffer.raw() + index;
        memset(buf, value, count);
    }

    int index_of_rev(char c) const;
    int index_of_rev(char c, int start) const;

    ByteBuffer substring(int start, int end) const;

    char *raw() {
        return _buffer.raw();
    }

    const char *raw() const {
        return _buffer.raw();
    }

    void fill(char value) {
        memset(_buffer.raw(), value, length());
    }

    void split(const char *split_by, List<ByteBuffer> &out) const;

    uint32_t hash() const {
        // FNV 32-bit hash
        uint32_t h = 2166136261;
        for (int i = 0; i < _buffer.length(); i += 1) {
            h = h ^ ((uint8_t)_buffer.at(i));
            h = h * 16777619;
        }
        return h;
    }

    int allocated_size() const {
        return _buffer.allocated_size();
    }

    static bool equal(const ByteBuffer &a, const ByteBuffer &b) {
        return (a.length() == b.length()) ? (compare(a, b) == 0) : false;
    }

    static int compare(const ByteBuffer &a, const ByteBuffer &b) {
        return memcmp(a.raw(), b.raw(), min(a.length(), b.length()) + 1);
    }

    static uint32_t hash(const ByteBuffer &key) {
        return key.hash();
    }

    inline bool operator==(const ByteBuffer &other) {
        return compare(*this, other) == 0;
    }

    inline bool operator!=(const ByteBuffer &other) {
        return compare(*this, other) != 0;
    }
private:
    List<char> _buffer;
};

#endif
