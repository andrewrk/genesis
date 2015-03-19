#include "sample_format.h"
#include "util.hpp"

int genesis_get_bytes_per_sample(enum GenesisSampleFormat sample_format) {
    switch (sample_format) {
    case GenesisSampleFormatUInt8: return 1;
    case GenesisSampleFormatInt16: return 2;
    case GenesisSampleFormatInt24: return 3;
    case GenesisSampleFormatInt32: return 4;
    case GenesisSampleFormatFloat: return 4;
    case GenesisSampleFormatDouble: return 8;
    case GenesisSampleFormatInvalid: panic("invalid sample format");
    }
    panic("invalid sample format");
}

const char * genesis_sample_format_string(enum GenesisSampleFormat sample_format) {
    switch (sample_format) {
    case GenesisSampleFormatUInt8: return "unsigned 8-bit integer";
    case GenesisSampleFormatInt16: return "signed 16-bit integer";
    case GenesisSampleFormatInt24: return "signed 24-bit integer";
    case GenesisSampleFormatInt32: return "signed 32-bit integer";
    case GenesisSampleFormatFloat: return "32-bit float";
    case GenesisSampleFormatDouble: return "64-bit float";
    case GenesisSampleFormatInvalid: return "invalid sample format";
    }
    panic("invalid sample format");
}
