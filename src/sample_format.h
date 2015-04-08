#ifndef GENESIS_SAMPLE_FORMAT_H
#define GENESIS_SAMPLE_FORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

// always native-endian
enum GenesisSampleFormat {
    GenesisSampleFormatUInt8,
    GenesisSampleFormatInt16,
    GenesisSampleFormatInt24,
    GenesisSampleFormatInt32,
    GenesisSampleFormatFloat,
    GenesisSampleFormatDouble,
    GenesisSampleFormatInvalid,
};

int genesis_get_bytes_per_sample(enum GenesisSampleFormat sample_format);

static inline int genesis_get_bytes_per_frame(enum GenesisSampleFormat sample_format, int channel_count) {
    return genesis_get_bytes_per_sample(sample_format) * channel_count;
}

static inline int genesis_get_bytes_per_second(enum GenesisSampleFormat sample_format,
        int channel_count, int sample_rate)
{
    return genesis_get_bytes_per_frame(sample_format, channel_count) * sample_rate;
}

const char * genesis_sample_format_string(enum GenesisSampleFormat sample_format);

#ifdef __cplusplus
}
#endif
#endif
