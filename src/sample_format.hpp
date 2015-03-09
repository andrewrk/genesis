#ifndef SAMPLE_FORMAT_H
#define SAMPLE_FORMAT_H

#include "util.hpp"

enum SampleFormat {
    SampleFormatUInt8,
    SampleFormatInt16,
    SampleFormatInt32,
    SampleFormatFloat,
    SampleFormatDouble,
    SampleFormatInvalid,
};

static inline int get_bytes_per_sample(SampleFormat sample_format) {
    switch (sample_format) {
    case SampleFormatUInt8: return 1;
    case SampleFormatInt16: return 2;
    case SampleFormatInt32: return 4;
    case SampleFormatFloat: return 4;
    case SampleFormatDouble: return 8;
    case SampleFormatInvalid: panic("invalid sample format");
    }
    panic("invalid sample format");
}

static inline int get_bytes_per_frame(SampleFormat sample_format, int channel_count) {
    return get_bytes_per_sample(sample_format) * channel_count;
}

static inline int get_bytes_per_second(SampleFormat sample_format, int channel_count, int sample_rate) {
    return get_bytes_per_frame(sample_format, channel_count) * sample_rate;
}

#endif
