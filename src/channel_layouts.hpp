#ifndef CHANNEL_LAYOUTS_HPP
#define CHANNEL_LAYOUTS_HPP

#include "string.hpp"
#include <stdint.h>

// must correlate to channel_names
enum ChannelId {
    ChannelIdFrontLeft,
    ChannelIdFrontRight,
    ChannelIdFrontCenter,
    ChannelIdLowFrequency,
    ChannelIdBackLeft,
    ChannelIdBackRight,
    ChannelIdFrontLeftOfCenter,
    ChannelIdFrontRightOfCenter,
    ChannelIdBackCenter,
    ChannelIdSideLeft,
    ChannelIdSideRight,
    ChannelIdTopCenter,
    ChannelIdTopFrontLeft,
    ChannelIdTopFrontCenter,
    ChannelIdTopFrontRight,
    ChannelIdTopBackLeft,
    ChannelIdTopBackCenter,
    ChannelIdTopBackRight,
    ChannelIdStereoLeft,
    ChannelIdStereoRight,
    ChannelIdWideLeft,
    ChannelIdWideRight,
    ChannelIdSurroundDirectLeft,
    ChannelIdSurroundDirectRight,
    ChannelIdLowFrequency2,
};

struct ChannelLayout {
    String name;
    List<ChannelId> channels;
    uint64_t libav_value;
};

// must correlate to initialize_channel_layouts
enum ChannelLayoutId {
    ChannelLayoutIdMono,
    ChannelLayoutIdStereo,
    ChannelLayoutId2Point1,
    ChannelLayoutId3Point0,
    ChannelLayoutId3Point0Back,
    ChannelLayoutId3Point1,
    ChannelLayoutId4Point0,
    ChannelLayoutId4Point1,
    ChannelLayoutIdQuad,
    ChannelLayoutIdQuadSide,
    ChannelLayoutId5Point0,
    ChannelLayoutId5Point0Back,
    ChannelLayoutId5Point1,
    ChannelLayoutId5Point1Back,
    ChannelLayoutId6Point0,
    ChannelLayoutId6Point0Front,
    ChannelLayoutIdHexagonal,
    ChannelLayoutId6Point1,
    ChannelLayoutId6Point1Back,
    ChannelLayoutId6Point1Front,
    ChannelLayoutId7Point0,
    ChannelLayoutId7Point0Front,
    ChannelLayoutId7Point1,
    ChannelLayoutId7Point1Wide,
    ChannelLayoutId7Point1WideBack,
    ChannelLayoutIdOctagonal,
    ChannelLayoutIdStereoDownmix,
};

void genesis_init_channel_layouts(void);
const char *genesis_get_channel_name(enum ChannelId id);
int genesis_get_channel_layout_count(void);
const struct ChannelLayout *genesis_get_channel_layout(int index);
const ChannelLayout *genesis_from_libav_channel_layout(uint64_t libav_channel_layout);
uint64_t genesis_to_libav_channel_layout(const ChannelLayout *channel_layout);

#endif
