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
ByteBuffer::ByteBuffer(const char * str, int length) {
    _buffer.append(0);
    append(str, length);
}

void ByteBuffer::append(const ByteBuffer &other) {
    append(other.raw(), other.length());
}

void ByteBuffer::append(const char *str) {
    append(str, strlen(str));
}

void ByteBuffer::append(const char *str, int length) {
    int prev_length_plus_null = _buffer.length();
    int new_length_plus_null = prev_length_plus_null + length;
    _buffer.resize(new_length_plus_null);
    memcpy(_buffer.raw() + prev_length_plus_null - 1, str, length);
    _buffer.at(new_length_plus_null - 1) = 0;
}

ByteBuffer ByteBuffer::format(const char *format, ...) {
    va_list ap, ap2;
    va_start(ap, format);
    va_copy(ap2, ap);

    int ret = vsnprintf(NULL, 0, format, ap);
    if (ret < 0)
        panic("vsnprintf error");

    int required_length = ret + 1;
    ByteBuffer result;
    result._buffer.resize(required_length);
    
    ret = vsnprintf(result._buffer.raw(), required_length, format, ap2);
    if (ret < 0)
        panic("vsnprintf error 2");

    va_end(ap2);
    va_end(ap);

    return result;
}

int ByteBuffer::index_of_rev(char c) const {
    return index_of_rev(c, length() - 1);
}

int ByteBuffer::index_of_rev(char c, int start) const {
    for (int i = start; i >= 0; i -= 1) {
        if (_buffer.at(i) == c)
            return i;
    }
    return -1;
}

ByteBuffer ByteBuffer::substring(int start, int end) const {
    if (start < 0 || start >= length())
        panic("substring start out of bounds");
    if (end < 0 || end > length())
        panic("substring end out of bounds");
    return ByteBuffer(_buffer.raw() + start, end - start);
}

ByteBuffer& ByteBuffer::operator= (const ByteBuffer& other) {
    if (this != &other) {
        _buffer.resize(other._buffer.length());
        memcpy(_buffer.raw(), other.raw(), _buffer.length());
    }
    return *this;
}

void ByteBuffer::split(const char *split_by, List<ByteBuffer> &out) const {
    const char *split_ptr = split_by;

    bool in_match = false;
    out.resize(out.length() + 1);
    ByteBuffer *current = &out.at(out.length() - 1);

    for (const char *buf_ptr = raw(); *buf_ptr; buf_ptr += 1) {
        if (*buf_ptr == *split_ptr) {
            in_match = true;
            split_ptr += 1;
            if (!*split_ptr) {
                split_ptr = split_by;
                in_match = false;
                out.resize(out.length() + 1);
                current = &out.at(out.length() - 1);
            }
            continue;
        } else if (in_match) {
            split_ptr = split_by;
            in_match = false;
        }
        current->append(buf_ptr, 1);
    }
}
