#ifndef STRING_HPP
#define STRING_HPP

#include "list.hpp"
#include "byte_buffer.hpp"

#include <stdint.h>

// UTF-8 string
class String {
public:
    String() {}
    String(const char *);
    String(const String &copy);
    String& operator= (const String& other);
    String(const ByteBuffer &bytes);

    static String decode(const ByteBuffer &bytes, bool &ok);
    // this one panics if the string is invalid
    static String decode(const ByteBuffer &bytes);
    ByteBuffer encode() const;

    int length() const {
        return _chars.length();
    }
    const uint32_t & at(int index) const {
        return _chars.at(index);
    }
    uint32_t & at(int index) {
        return _chars.at(index);
    }

    static const int max_codepoint = 0x1fffff;
    void append(uint32_t c) {
        if (c > max_codepoint)
            panic("codepoint out of range");
        _chars.append(c);
    }

private:
    List<uint32_t> _chars;
};

#endif
