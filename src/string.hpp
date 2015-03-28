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
    String(const ByteBuffer &bytes, bool *ok);

    static String decode(const ByteBuffer &bytes, bool *ok);
    // this one panics if the string is invalid
    static String decode(const ByteBuffer &bytes);
    ByteBuffer encode() const;

    long length() const {
        return _chars.length();
    }
    const uint32_t & at(long index) const {
        return _chars.at(index);
    }
    uint32_t & at(long index) {
        return _chars.at(index);
    }

    void make_lower_case();
    void make_upper_case();

    static const int max_codepoint = 0x1fffff;
    void append(uint32_t c) {
        if (c > max_codepoint)
            panic("codepoint out of range");
        if (_chars.append(c))
            panic("out of memory");
    }
    String substring(long start, long end) const;
    String substring(long start) const;

    void replace(long start, long end, String s);

    void split_on_whitespace(List<String> &out) const;

    off_t index_of_insensitive(const String &other) const;

    static bool equal(const String &a, const String &b);
    static int compare(const String &a, const String &b);
    static int compare_insensitive(const String &a, const String &b);
    static uint32_t char_to_lower(uint32_t c);
    static uint32_t char_to_upper(uint32_t c);

    static bool is_whitespace(uint32_t c);

private:
    List<uint32_t> _chars;
};

#endif
