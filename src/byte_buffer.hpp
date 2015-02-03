#ifndef BYTE_BUFFER
#define BYTE_BUFFER

#include "list.hpp"
#include <stdio.h>

class ByteBuffer {
public:
    ByteBuffer();
    ByteBuffer(const ByteBuffer & copy);
    ByteBuffer(const char * str);
    ByteBuffer(const char * str, int size);
    ~ByteBuffer() {}
    ByteBuffer& operator= (const ByteBuffer& other);

    const char & at(int index) const {
        return _buffer.at(index);
    }
    char & at(int index) {
        return _buffer.at(index);
    }

    static ByteBuffer format(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

    int size() const {
        return _buffer.size() - 1;
    }
    void resize(int size) {
        _buffer.resize(size + 1);
    }
    void append(const ByteBuffer &other);
    void append(const char *str);
    void append(const char *str, int size);

    int index_of_rev(char c) const;
    int index_of_rev(char c, int start) const;

    ByteBuffer substring(int start, int end) const;

    char *raw() const {
        return _buffer.raw();
    }
private:
    List<char> _buffer;
};

#endif
