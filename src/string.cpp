#include "string.hpp"
#include "unicode.hpp"

static void assert_no_err(int err) {
    if (err)
        panic("%s", genesis_strerror(err));
}

String::String(const ByteBuffer &bytes) {
    *this = decode(bytes);
}

String::String(const ByteBuffer &bytes, bool *ok) {
    *this = decode(bytes, ok);
}

String::String(const String &copy) {
    *this = copy;
}

String::String(const char *str) : String(ByteBuffer(str)) {}

String& String::operator= (const String &other) {
    assert_no_err(_chars.resize(other.length()));
    for (int i = 0; i < _chars.length(); i += 1) {
        _chars.at(i) = other._chars.at(i);
    }
    return *this;
}

String String::decode(const ByteBuffer &bytes) {
    bool ok;
    String str = String::decode(bytes, &ok);
    if (!ok)
        panic("invalid UTF-8");
    return str;
}

String String::decode(const ByteBuffer &bytes, bool *ok) {
    String str;
    *ok = true;
    for (int i = 0; i < bytes.length(); i += 1) {
        uint8_t byte1 = *((uint8_t*)&bytes.at(i));
        if ((0x80 & byte1) == 0) {
            str.append(byte1);
        } else if ((0xe0 & byte1) == 0xc0) {
            if (++i >= bytes.length()) {
                *ok = false;
                str.append(0xfffd);
                break;
            }
            uint8_t byte2 = *((uint8_t*)&bytes.at(i));
            if ((byte2 & 0xc0) != 0x80) {
                *ok = false;
                str.append(0xfffd);
                continue;
            }

            uint32_t bits_byte1 = (byte1 & 0x1f);
            uint32_t bits_byte2 = (byte2 & 0x3f);
            str.append(bits_byte2 | (bits_byte1 << 6));
        } else if ((0xf0 & byte1) == 0xe0) {
            if (++i >= bytes.length()) {
                *ok = false;
                str.append(0xfffd);
                break;
            }
            uint8_t byte2 = *((uint8_t*)&bytes.at(i));
            if ((byte2 & 0xc0) != 0x80) {
                *ok = false;
                str.append(0xfffd);
                continue;
            }

            if (++i >= bytes.length()) {
                *ok = false;
                str.append(0xfffd);
                break;
            }
            uint8_t byte3 = *((uint8_t*)&bytes.at(i));
            if ((byte3 & 0xc0) != 0x80) {
                *ok = false;
                str.append(0xfffd);
                continue;
            }

            uint32_t bits_byte1 = (byte1 & 0xf);
            uint32_t bits_byte2 = (byte2 & 0x3f);
            uint32_t bits_byte3 = (byte3 & 0x3f);
            str.append(bits_byte3 | (bits_byte2 << 6) | (bits_byte1 << 12));
        } else if ((0xf8 & byte1) == 0xf0) {
            if (++i >= bytes.length()) {
                *ok = false;
                str.append(0xfffd);
                break;
            }
            uint8_t byte2 = *((uint8_t*)&bytes.at(i));
            if ((byte2 & 0xc0) != 0x80) {
                *ok = false;
                str.append(0xfffd);
                continue;
            }

            if (++i >= bytes.length()) {
                *ok = false;
                str.append(0xfffd);
                break;
            }
            uint8_t byte3 = *((uint8_t*)&bytes.at(i));
            if ((byte3 & 0xc0) != 0x80) {
                *ok = false;
                str.append(0xfffd);
                continue;
            }

            if (++i >= bytes.length()) {
                *ok = false;
                str.append(0xfffd);
                break;
            }
            uint8_t byte4 = *((uint8_t*)&bytes.at(i));
            if ((byte3 & 0xc0) != 0x80) {
                *ok = false;
                str.append(0xfffd);
                continue;
            }

            uint32_t bits_byte1 = (byte1 & 0x7);
            uint32_t bits_byte2 = (byte2 & 0x3f);
            uint32_t bits_byte3 = (byte3 & 0x3f);
            uint32_t bits_byte4 = (byte4 & 0x3f);
            str.append(bits_byte4 | (bits_byte3 << 6) | (bits_byte2 << 12) | (bits_byte1 << 18));
        } else {
            *ok = false;
            str.append(0xfffd);
        }
    }
    return str;
}

ByteBuffer String::encode() const {
    ByteBuffer result;
    for (int i = 0; i < _chars.length(); i += 1) {
        uint32_t codepoint = _chars.at(i);
        if (codepoint <= 0x7f) {
            // 00000000 00000000 00000000 0xxxxxxx
            unsigned char bytes[1] = {
                (unsigned char)(codepoint),
            };
            result.append((char*)bytes, 1);
        } else if (codepoint <= 0x7ff) {
            unsigned char bytes[2] = {
                // 00000000 00000000 00000xxx xx000000
                (unsigned char)(0xc0 | (codepoint >> 6)),
                // 00000000 00000000 00000000 00xxxxxx
                (unsigned char)(0x80 | (codepoint & 0x3f)),
            };
            result.append((char*)bytes, 2);
        } else if (codepoint <= 0xffff) {
            unsigned char bytes[3] = {
                // 00000000 00000000 xxxx0000 00000000
                (unsigned char)(0xe0 | (codepoint >> 12)),
                // 00000000 00000000 0000xxxx xx000000
                (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)),
                // 00000000 00000000 00000000 00xxxxxx
                (unsigned char)(0x80 | (codepoint & 0x3f)),
            };
            result.append((char*)bytes, 3);
        } else if (codepoint <= 0x1fffff) {
            unsigned char bytes[4] = {
                // 00000000 000xxx00 00000000 00000000
                (unsigned char)(0xf0 | (codepoint >> 18)),
                // 00000000 000000xx xxxx0000 00000000
                (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f)),
                // 00000000 00000000 0000xxxx xx000000
                (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)),
                // 00000000 00000000 00000000 00xxxxxx
                (unsigned char)(0x80 | (codepoint & 0x3f)),
            };
            result.append((char*)bytes, 4);
        } else {
            panic("codepoint out of UTF8 range");
        }
    }
    return result;
}

String String::substring(int start, int end) const {
    String result;
    for (int i = start; i < end; i += 1) {
        result.append(_chars.at(i));
    }
    return result;
}

String String::substring(int start) const {
    return substring(start, _chars.length());
}

void String::replace(int start, int end, String s) {
    String second_half = substring(end);
    assert_no_err(_chars.resize(_chars.length() + s.length() - (end - start)));
    for (int i = 0; i < s.length(); i += 1) {
        _chars.at(start + i) = s.at(i);
    }
    for (int i = 0; i < second_half.length(); i += 1) {
        _chars.at(start + s.length() + i) = second_half.at(i);
    }
}

void String::append(String s) {
    int start = _chars.length();
    assert_no_err(_chars.resize(_chars.length() + s.length()));
    for (int i = 0; i < s.length(); i += 1) {
        _chars.at(start + i) = s.at(i);
    }
}

int String::compare(const String &a, const String &b) {
    for (int i = 0;; i += 1) {
        bool a_end = (i >= a.length());
        bool b_end = (i >= b.length());
        if (a_end) {
            if (b_end)
                return 0;
            else
                return -1;
        } else if (b_end) {
            return 1;
        } else {
            if (a.at(i) > b.at(i))
                return 1;
            else if (a.at(i) < b.at(i))
                return -1;
            else
                continue;
        }
    }
}

int String::compare_insensitive(const String &a, const String &b) {
    for (int i = 0;; i += 1) {
        bool a_end = (i >= a.length());
        bool b_end = (i >= b.length());
        if (a_end) {
            if (b_end) {
                return 0;
            } else {
                return -1;
            }
        } else if (b_end) {
            return 1;
        } else {
            uint32_t a_lower = char_to_lower(a.at(i));
            uint32_t b_lower = char_to_lower(b.at(i));
            if (a_lower > b_lower)
                return 1;
            else if (a_lower < b_lower)
                return -1;
            else
                continue;
        }
    }
}

void String::make_lower_case() {
    for (int i = 0; i < _chars.length(); i += 1) {
        _chars.at(i) = char_to_lower(_chars.at(i));
    }
}

void String::make_upper_case() {
    for (int i = 0; i < _chars.length(); i += 1) {
        _chars.at(i) = char_to_upper(_chars.at(i));
    }
}

uint32_t String::char_to_lower(uint32_t c) {
    return (c < array_length(unicode_characters)) ? unicode_characters[c].lower : c;
}

uint32_t String::char_to_upper(uint32_t c) {
    return (c < array_length(unicode_characters)) ? unicode_characters[c].upper : c;
}

bool String::is_whitespace(uint32_t c) {
    for (int i = 0; i < array_length(whitespace); i+= 1) {
        if (c == whitespace[i])
            return true;
    }
    return false;
}

void String::split_on_whitespace(List<String> &out) const {
    assert_no_err(out.resize(1));
    String *current = &out.at(0);

    for (int i = 0; i < _chars.length(); i += 1) {
        uint32_t c = _chars.at(i);
        if (is_whitespace(c)) {
            assert_no_err(out.resize(out.length() + 1));
            current = &out.at(out.length() - 1);
            continue;
        }
        current->append(c);
    }
}

off_t String::index_of_insensitive(const String &search) const {
    off_t upper_bound = _chars.length() - search._chars.length() + 1;
    for (off_t i = 0; i < upper_bound; i += 1) {
        bool all_ok = true;
        for (int inner = 0; inner < search._chars.length(); inner += 1) {
            uint32_t lower_a = char_to_lower(_chars.at(i + inner));
            uint32_t lower_b = char_to_lower(search.at(inner));
            if (lower_a != lower_b) {
                all_ok = false;
                break;
            }
        }
        if (all_ok)
            return i;
    }
    return -1;
}

bool String::equal(const String &a, const String &b) {
    return (a._chars.length() == b._chars.length()) ? (compare(a, b) == 0) : false;
}
