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
