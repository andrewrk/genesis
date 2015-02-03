#include "byte_buffer.hpp"

#include "util.hpp"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

ByteBuffer::ByteBuffer() {
    _buffer.append(0);
}

ByteBuffer::ByteBuffer(const char * str) {
    _buffer.append(0);
    append(str, strlen(str));
}
ByteBuffer::ByteBuffer(const ByteBuffer & copy)
{
    _buffer.append(0);
    append(copy);
}
ByteBuffer::ByteBuffer(const char * str, int size) {
    _buffer.append(0);
    append(str, size);
}

void ByteBuffer::append(const ByteBuffer &other) {
    append(other.raw(), other.size());
}

void ByteBuffer::append(const char *str) {
    append(str, strlen(str));
}

void ByteBuffer::append(const char *str, int size) {
    int prev_size_plus_null = _buffer.size();
    int new_size_plus_null = prev_size_plus_null + size;
    _buffer.resize(new_size_plus_null);
    memcpy(_buffer.raw() + prev_size_plus_null - 1, str, size);
    _buffer.at(new_size_plus_null - 1) = 0;
}

ByteBuffer ByteBuffer::format(const char *format, ...) {
    va_list ap, ap2;
    va_start(ap, format);
    va_copy(ap2, ap);

    int ret = vsnprintf(NULL, 0, format, ap);
    if (ret < 0)
        panic("vsnprintf error");

    int required_size = ret + 1;
    ByteBuffer result;
    result._buffer.resize(required_size);
    
    ret = vsnprintf(result._buffer.raw(), required_size, format, ap2);
    if (ret < 0)
        panic("vsnprintf error 2");

    va_end(ap2);
    va_end(ap);

    return result;
}

int ByteBuffer::index_of_rev(char c) const {
    return index_of_rev(c, size() - 1);
}

int ByteBuffer::index_of_rev(char c, int start) const {
    for (int i = start; i >= 0; i -= 1) {
        if (_buffer.at(i) == c)
            return i;
    }
    return -1;
}

ByteBuffer ByteBuffer::substring(int start, int end) const {
    if (start < 0 || start >= size())
        panic("substring start out of bounds");
    if (end < 0 || end > size())
        panic("substring end out of bounds");
    return ByteBuffer(_buffer.raw() + start, end - start);
}

ByteBuffer& ByteBuffer::operator= (const ByteBuffer& other) {
    if (this != &other) {
        _buffer.resize(other._buffer.size());
        memcpy(_buffer.raw(), other.raw(), _buffer.size());
    }
    return *this;
}
