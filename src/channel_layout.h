#ifndef GENESIS_CHANNEL_LAYOUT_H
#define GENESIS_CHANNEL_LAYOUT_H

#include "error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// must correlate to channel_names
enum GenesisChannelId {
    GenesisChannelIdInvalid,
    GenesisChannelIdFrontLeft,
    GenesisChannelIdFrontRight,
    GenesisChannelIdFrontCenter,
    GenesisChannelIdLowFrequency,
    GenesisChannelIdBackLeft,
    GenesisChannelIdBackRight,
    GenesisChannelIdFrontLeftOfCenter,
    GenesisChannelIdFrontRightOfCenter,
    GenesisChannelIdBackCenter,
    GenesisChannelIdSideLeft,
    GenesisChannelIdSideRight,
    GenesisChannelIdTopCenter,
    GenesisChannelIdTopFrontLeft,
    GenesisChannelIdTopFrontCenter,
    GenesisChannelIdTopFrontRight,
    GenesisChannelIdTopBackLeft,
    GenesisChannelIdTopBackCenter,
    GenesisChannelIdTopBackRight,
};

#define GENESIS_MAX_CHANNELS 32
struct GenesisChannelLayout {
    const char *name;
    int channel_count;
    enum GenesisChannelId channels[GENESIS_MAX_CHANNELS];
};

enum GenesisChannelLayoutId {
    GenesisChannelLayoutIdMono,
    GenesisChannelLayoutIdStereo,
    GenesisChannelLayoutId2Point1,
    GenesisChannelLayoutId3Point0,
    GenesisChannelLayoutId3Point0Back,
    GenesisChannelLayoutId3Point1,
    GenesisChannelLayoutId4Point0,
    GenesisChannelLayoutId4Point1,
    GenesisChannelLayoutIdQuad,
    GenesisChannelLayoutIdQuadSide,
    GenesisChannelLayoutId5Point0,
    GenesisChannelLayoutId5Point0Back,
    GenesisChannelLayoutId5Point1,
    GenesisChannelLayoutId5Point1Back,
    GenesisChannelLayoutId6Point0,
    GenesisChannelLayoutId6Point0Front,
    GenesisChannelLayoutIdHexagonal,
    GenesisChannelLayoutId6Point1,
    GenesisChannelLayoutId6Point1Back,
    GenesisChannelLayoutId6Point1Front,
    GenesisChannelLayoutId7Point0,
    GenesisChannelLayoutId7Point0Front,
    GenesisChannelLayoutId7Point1,
    GenesisChannelLayoutId7Point1Wide,
    GenesisChannelLayoutId7Point1WideBack,
    GenesisChannelLayoutIdOctagonal,
};

bool genesis_channel_layout_equal(const struct GenesisChannelLayout *a,
        const struct GenesisChannelLayout *b);

const char *genesis_get_channel_name(enum GenesisChannelId id);

int genesis_channel_layout_builtin_count(void);
const struct GenesisChannelLayout *genesis_channel_layout_get_builtin(int index);

void genesis_debug_print_channel_layout(const struct GenesisChannelLayout *layout);

#ifdef __cplusplus
}
#endif
#endif
