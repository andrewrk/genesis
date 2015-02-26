#include "channel_layouts.hpp"
#include "list.hpp"
#include "libav.hpp"

// must correlate to ChannelId enum
static const char * channel_names[] = {
    "Front Left",
    "Front Right",
    "Front Center",
    "Low Frequency",
    "Back Left",
    "Back Right",
    "Front Left of Center",
    "Front Right of Center",
    "Back Center",
    "Side Left",
    "Side Right",
    "Top Center",
    "Top Front Left",
    "Top Front Center",
    "Top Front Right",
    "Top Back Left",
    "Top Back Center",
    "Top Back Right",
    "Stereo Left",
    "Stereo Right",
    "Wide Left",
    "Wide Right",
};

static List<ChannelLayout> channel_layouts;

const ChannelLayout *genesis_from_libav_channel_layout(uint64_t libav_channel_layout) {
    for (size_t i = 0; i < channel_layouts.length(); i += 1) {
        const ChannelLayout *layout = &channel_layouts.at(i);
        if (layout->libav_value == libav_channel_layout)
            return layout;
    }
    return NULL;
}

// must correlate to enum ChannelLayoutId
void genesis_init_channel_layouts(void) {
    channel_layouts.resize(27);

    channel_layouts.at(0).name = "Mono";
    channel_layouts.at(0).libav_value = AV_CH_LAYOUT_MONO;
    channel_layouts.at(0).channels.append(ChannelIdFrontCenter);

    channel_layouts.at(1).name = "Stereo";
    channel_layouts.at(1).libav_value = AV_CH_LAYOUT_STEREO;
    channel_layouts.at(1).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(1).channels.append(ChannelIdFrontRight);

    channel_layouts.at(2).name = "2.1";
    channel_layouts.at(2).libav_value = AV_CH_LAYOUT_2POINT1;
    channel_layouts.at(2).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(2).channels.append(ChannelIdFrontRight);
    channel_layouts.at(2).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(3).name = "3.0";
    channel_layouts.at(3).libav_value = AV_CH_LAYOUT_MONO;
    channel_layouts.at(3).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(3).channels.append(ChannelIdFrontRight);
    channel_layouts.at(3).channels.append(ChannelIdFrontCenter);

    channel_layouts.at(4).name = "3.0 (back)";
    channel_layouts.at(4).libav_value = AV_CH_LAYOUT_2_1;
    channel_layouts.at(4).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(4).channels.append(ChannelIdFrontRight);
    channel_layouts.at(4).channels.append(ChannelIdBackCenter);

    channel_layouts.at(5).name = "3.1";
    channel_layouts.at(5).libav_value = AV_CH_LAYOUT_3POINT1;
    channel_layouts.at(5).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(5).channels.append(ChannelIdFrontRight);
    channel_layouts.at(5).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(5).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(6).name = "4.0";
    channel_layouts.at(6).libav_value = AV_CH_LAYOUT_4POINT0;
    channel_layouts.at(6).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(6).channels.append(ChannelIdFrontRight);
    channel_layouts.at(6).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(6).channels.append(ChannelIdBackCenter);

    channel_layouts.at(7).name = "4.1";
    channel_layouts.at(7).libav_value = AV_CH_LAYOUT_4POINT1;
    channel_layouts.at(7).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(7).channels.append(ChannelIdFrontRight);
    channel_layouts.at(7).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(7).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(8).name = "Quad";
    channel_layouts.at(8).libav_value = AV_CH_LAYOUT_QUAD;
    channel_layouts.at(8).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(8).channels.append(ChannelIdFrontRight);
    channel_layouts.at(8).channels.append(ChannelIdBackLeft);
    channel_layouts.at(8).channels.append(ChannelIdBackRight);

    channel_layouts.at(9).name = "Quad (side)";
    channel_layouts.at(9).libav_value = AV_CH_LAYOUT_2_2;
    channel_layouts.at(9).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(9).channels.append(ChannelIdFrontRight);
    channel_layouts.at(9).channels.append(ChannelIdSideLeft);
    channel_layouts.at(9).channels.append(ChannelIdSideRight);

    channel_layouts.at(10).name = "5.0";
    channel_layouts.at(10).libav_value = AV_CH_LAYOUT_5POINT0;
    channel_layouts.at(10).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(10).channels.append(ChannelIdFrontRight);
    channel_layouts.at(10).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(10).channels.append(ChannelIdSideLeft);
    channel_layouts.at(10).channels.append(ChannelIdSideRight);

    channel_layouts.at(11).name = "5.0 (back)";
    channel_layouts.at(11).libav_value = AV_CH_LAYOUT_5POINT0_BACK;
    channel_layouts.at(11).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(11).channels.append(ChannelIdFrontRight);
    channel_layouts.at(11).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(11).channels.append(ChannelIdBackLeft);
    channel_layouts.at(11).channels.append(ChannelIdBackRight);

    channel_layouts.at(12).name = "5.1";
    channel_layouts.at(12).libav_value = AV_CH_LAYOUT_5POINT1;
    channel_layouts.at(12).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(12).channels.append(ChannelIdFrontRight);
    channel_layouts.at(12).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(12).channels.append(ChannelIdSideLeft);
    channel_layouts.at(12).channels.append(ChannelIdSideRight);
    channel_layouts.at(12).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(13).name = "5.1 (back)";
    channel_layouts.at(13).libav_value = AV_CH_LAYOUT_5POINT1_BACK;
    channel_layouts.at(13).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(13).channels.append(ChannelIdFrontRight);
    channel_layouts.at(13).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(13).channels.append(ChannelIdBackLeft);
    channel_layouts.at(13).channels.append(ChannelIdBackRight);
    channel_layouts.at(13).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(14).name = "6.0";
    channel_layouts.at(14).libav_value = AV_CH_LAYOUT_6POINT0;
    channel_layouts.at(14).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(14).channels.append(ChannelIdFrontRight);
    channel_layouts.at(14).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(14).channels.append(ChannelIdSideLeft);
    channel_layouts.at(14).channels.append(ChannelIdSideRight);
    channel_layouts.at(14).channels.append(ChannelIdBackCenter);

    channel_layouts.at(15).name = "6.0 (front)";
    channel_layouts.at(15).libav_value = AV_CH_LAYOUT_6POINT0_FRONT;
    channel_layouts.at(15).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(15).channels.append(ChannelIdFrontRight);
    channel_layouts.at(15).channels.append(ChannelIdSideLeft);
    channel_layouts.at(15).channels.append(ChannelIdSideRight);
    channel_layouts.at(15).channels.append(ChannelIdFrontLeftOfCenter);
    channel_layouts.at(15).channels.append(ChannelIdFrontRightOfCenter);

    channel_layouts.at(16).name = "Hexagonal";
    channel_layouts.at(16).libav_value = AV_CH_LAYOUT_HEXAGONAL;
    channel_layouts.at(16).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(16).channels.append(ChannelIdFrontRight);
    channel_layouts.at(16).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(16).channels.append(ChannelIdBackLeft);
    channel_layouts.at(16).channels.append(ChannelIdBackRight);
    channel_layouts.at(16).channels.append(ChannelIdBackCenter);

    channel_layouts.at(17).name = "6.1";
    channel_layouts.at(17).libav_value = AV_CH_LAYOUT_6POINT1;
    channel_layouts.at(17).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(17).channels.append(ChannelIdFrontRight);
    channel_layouts.at(17).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(17).channels.append(ChannelIdSideLeft);
    channel_layouts.at(17).channels.append(ChannelIdSideRight);
    channel_layouts.at(17).channels.append(ChannelIdBackCenter);
    channel_layouts.at(17).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(18).name = "6.1 (back)";
    channel_layouts.at(18).libav_value = AV_CH_LAYOUT_6POINT1_BACK;
    channel_layouts.at(18).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(18).channels.append(ChannelIdFrontRight);
    channel_layouts.at(18).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(18).channels.append(ChannelIdBackLeft);
    channel_layouts.at(18).channels.append(ChannelIdBackRight);
    channel_layouts.at(18).channels.append(ChannelIdBackCenter);
    channel_layouts.at(18).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(19).name = "6.1 (front)";
    channel_layouts.at(19).libav_value = AV_CH_LAYOUT_6POINT1_FRONT;
    channel_layouts.at(19).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(19).channels.append(ChannelIdFrontRight);
    channel_layouts.at(19).channels.append(ChannelIdSideLeft);
    channel_layouts.at(19).channels.append(ChannelIdSideRight);
    channel_layouts.at(19).channels.append(ChannelIdFrontLeftOfCenter);
    channel_layouts.at(19).channels.append(ChannelIdFrontRightOfCenter);
    channel_layouts.at(19).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(20).name = "7.0";
    channel_layouts.at(20).libav_value = AV_CH_LAYOUT_7POINT0;
    channel_layouts.at(20).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(20).channels.append(ChannelIdFrontRight);
    channel_layouts.at(20).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(20).channels.append(ChannelIdSideLeft);
    channel_layouts.at(20).channels.append(ChannelIdSideRight);
    channel_layouts.at(20).channels.append(ChannelIdBackLeft);
    channel_layouts.at(20).channels.append(ChannelIdBackRight);

    channel_layouts.at(21).name = "7.0 (front)";
    channel_layouts.at(21).libav_value = AV_CH_LAYOUT_7POINT0_FRONT;
    channel_layouts.at(21).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(21).channels.append(ChannelIdFrontRight);
    channel_layouts.at(21).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(21).channels.append(ChannelIdSideLeft);
    channel_layouts.at(21).channels.append(ChannelIdSideRight);
    channel_layouts.at(21).channels.append(ChannelIdFrontLeftOfCenter);
    channel_layouts.at(21).channels.append(ChannelIdFrontRightOfCenter);

    channel_layouts.at(22).name = "7.1";
    channel_layouts.at(22).libav_value = AV_CH_LAYOUT_7POINT1;
    channel_layouts.at(22).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(22).channels.append(ChannelIdFrontRight);
    channel_layouts.at(22).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(22).channels.append(ChannelIdSideLeft);
    channel_layouts.at(22).channels.append(ChannelIdSideRight);
    channel_layouts.at(22).channels.append(ChannelIdBackLeft);
    channel_layouts.at(22).channels.append(ChannelIdBackRight);
    channel_layouts.at(22).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(23).name = "7.1 (wide)";
    channel_layouts.at(23).libav_value = AV_CH_LAYOUT_7POINT1_WIDE;
    channel_layouts.at(23).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(23).channels.append(ChannelIdFrontRight);
    channel_layouts.at(23).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(23).channels.append(ChannelIdSideLeft);
    channel_layouts.at(23).channels.append(ChannelIdSideRight);
    channel_layouts.at(23).channels.append(ChannelIdFrontLeftOfCenter);
    channel_layouts.at(23).channels.append(ChannelIdFrontRightOfCenter);
    channel_layouts.at(23).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(24).name = "7.1 (wide) (back)";
    channel_layouts.at(24).libav_value = AV_CH_LAYOUT_7POINT1_WIDE_BACK;
    channel_layouts.at(24).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(24).channels.append(ChannelIdFrontRight);
    channel_layouts.at(24).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(24).channels.append(ChannelIdBackLeft);
    channel_layouts.at(24).channels.append(ChannelIdBackRight);
    channel_layouts.at(24).channels.append(ChannelIdFrontLeftOfCenter);
    channel_layouts.at(24).channels.append(ChannelIdFrontRightOfCenter);
    channel_layouts.at(24).channels.append(ChannelIdLowFrequency);

    channel_layouts.at(25).name = "Octagonal";
    channel_layouts.at(25).libav_value = AV_CH_LAYOUT_OCTAGONAL;
    channel_layouts.at(25).channels.append(ChannelIdFrontLeft);
    channel_layouts.at(25).channels.append(ChannelIdFrontRight);
    channel_layouts.at(25).channels.append(ChannelIdFrontCenter);
    channel_layouts.at(25).channels.append(ChannelIdSideLeft);
    channel_layouts.at(25).channels.append(ChannelIdSideRight);
    channel_layouts.at(25).channels.append(ChannelIdBackLeft);
    channel_layouts.at(25).channels.append(ChannelIdBackRight);
    channel_layouts.at(25).channels.append(ChannelIdBackCenter);

    channel_layouts.at(26).name = "Stereo Downmix";
    channel_layouts.at(26).libav_value = AV_CH_LAYOUT_STEREO_DOWNMIX;
    channel_layouts.at(26).channels.append(ChannelIdStereoLeft);
    channel_layouts.at(26).channels.append(ChannelIdStereoRight);
}

const char *genesis_get_channel_name(enum ChannelId id) {
    if (id < 0 || id >= array_length(channel_names))
        panic("invalid channel id");
    return channel_names[id];
}

const struct ChannelLayout *genesis_get_channel_layout(int index) {
    return &channel_layouts.at(index);
}

uint64_t genesis_to_libav_channel_layout(const ChannelLayout *channel_layout) {
    return channel_layout->libav_value;
}
