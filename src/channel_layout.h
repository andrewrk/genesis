#ifndef GENESIS_CHANNEL_LAYOUT_H
#define GENESIS_CHANNEL_LAYOUT_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stdint.h>

/* Audio channel masks */
#define GENESIS_CH_FRONT_LEFT             0x00000001
#define GENESIS_CH_FRONT_RIGHT            0x00000002
#define GENESIS_CH_FRONT_CENTER           0x00000004
#define GENESIS_CH_LOW_FREQUENCY          0x00000008
#define GENESIS_CH_BACK_LEFT              0x00000010
#define GENESIS_CH_BACK_RIGHT             0x00000020
#define GENESIS_CH_FRONT_LEFT_OF_CENTER   0x00000040
#define GENESIS_CH_FRONT_RIGHT_OF_CENTER  0x00000080
#define GENESIS_CH_BACK_CENTER            0x00000100
#define GENESIS_CH_SIDE_LEFT              0x00000200
#define GENESIS_CH_SIDE_RIGHT             0x00000400
#define GENESIS_CH_TOP_CENTER             0x00000800
#define GENESIS_CH_TOP_FRONT_LEFT         0x00001000
#define GENESIS_CH_TOP_FRONT_CENTER       0x00002000
#define GENESIS_CH_TOP_FRONT_RIGHT        0x00004000
#define GENESIS_CH_TOP_BACK_LEFT          0x00008000
#define GENESIS_CH_TOP_BACK_CENTER        0x00010000
#define GENESIS_CH_TOP_BACK_RIGHT         0x00020000
#define GENESIS_CH_STEREO_LEFT            0x20000000  ///< Stereo downmix.
#define GENESIS_CH_STEREO_RIGHT           0x40000000  ///< See GENESIS_CH_STEREO_LEFT.
#define GENESIS_CH_WIDE_LEFT              0x0000000080000000ULL
#define GENESIS_CH_WIDE_RIGHT             0x0000000100000000ULL
#define GENESIS_CH_SURROUND_DIRECT_LEFT   0x0000000200000000ULL
#define GENESIS_CH_SURROUND_DIRECT_RIGHT  0x0000000400000000ULL
#define GENESIS_CH_LOW_FREQUENCY_2        0x0000000800000000ULL


/* Audio channel convenience macros */
#define GENESIS_CH_LAYOUT_MONO              (GENESIS_CH_FRONT_CENTER)
#define GENESIS_CH_LAYOUT_STEREO            (GENESIS_CH_FRONT_LEFT|GENESIS_CH_FRONT_RIGHT)
#define GENESIS_CH_LAYOUT_2POINT1           (GENESIS_CH_LAYOUT_STEREO|GENESIS_CH_LOW_FREQUENCY)
#define GENESIS_CH_LAYOUT_2_1               (GENESIS_CH_LAYOUT_STEREO|GENESIS_CH_BACK_CENTER)
#define GENESIS_CH_LAYOUT_SURROUND          (GENESIS_CH_LAYOUT_STEREO|GENESIS_CH_FRONT_CENTER)
#define GENESIS_CH_LAYOUT_3POINT1           (GENESIS_CH_LAYOUT_SURROUND|GENESIS_CH_LOW_FREQUENCY)
#define GENESIS_CH_LAYOUT_4POINT0           (GENESIS_CH_LAYOUT_SURROUND|GENESIS_CH_BACK_CENTER)
#define GENESIS_CH_LAYOUT_4POINT1           (GENESIS_CH_LAYOUT_4POINT0|GENESIS_CH_LOW_FREQUENCY)
#define GENESIS_CH_LAYOUT_2_2               (GENESIS_CH_LAYOUT_STEREO|GENESIS_CH_SIDE_LEFT|GENESIS_CH_SIDE_RIGHT)
#define GENESIS_CH_LAYOUT_QUAD              (GENESIS_CH_LAYOUT_STEREO|GENESIS_CH_BACK_LEFT|GENESIS_CH_BACK_RIGHT)
#define GENESIS_CH_LAYOUT_5POINT0           (GENESIS_CH_LAYOUT_SURROUND|GENESIS_CH_SIDE_LEFT|GENESIS_CH_SIDE_RIGHT)
#define GENESIS_CH_LAYOUT_5POINT1           (GENESIS_CH_LAYOUT_5POINT0|GENESIS_CH_LOW_FREQUENCY)
#define GENESIS_CH_LAYOUT_5POINT0_BACK      (GENESIS_CH_LAYOUT_SURROUND|GENESIS_CH_BACK_LEFT|GENESIS_CH_BACK_RIGHT)
#define GENESIS_CH_LAYOUT_5POINT1_BACK      (GENESIS_CH_LAYOUT_5POINT0_BACK|GENESIS_CH_LOW_FREQUENCY)
#define GENESIS_CH_LAYOUT_6POINT0           (GENESIS_CH_LAYOUT_5POINT0|GENESIS_CH_BACK_CENTER)
#define GENESIS_CH_LAYOUT_6POINT0_FRONT     (GENESIS_CH_LAYOUT_2_2|GENESIS_CH_FRONT_LEFT_OF_CENTER|GENESIS_CH_FRONT_RIGHT_OF_CENTER)
#define GENESIS_CH_LAYOUT_HEXAGONAL         (GENESIS_CH_LAYOUT_5POINT0_BACK|GENESIS_CH_BACK_CENTER)
#define GENESIS_CH_LAYOUT_6POINT1           (GENESIS_CH_LAYOUT_5POINT1|GENESIS_CH_BACK_CENTER)
#define GENESIS_CH_LAYOUT_6POINT1_BACK      (GENESIS_CH_LAYOUT_5POINT1_BACK|GENESIS_CH_BACK_CENTER)
#define GENESIS_CH_LAYOUT_6POINT1_FRONT     (GENESIS_CH_LAYOUT_6POINT0_FRONT|GENESIS_CH_LOW_FREQUENCY)
#define GENESIS_CH_LAYOUT_7POINT0           (GENESIS_CH_LAYOUT_5POINT0|GENESIS_CH_BACK_LEFT|GENESIS_CH_BACK_RIGHT)
#define GENESIS_CH_LAYOUT_7POINT0_FRONT     (GENESIS_CH_LAYOUT_5POINT0|GENESIS_CH_FRONT_LEFT_OF_CENTER|GENESIS_CH_FRONT_RIGHT_OF_CENTER)
#define GENESIS_CH_LAYOUT_7POINT1           (GENESIS_CH_LAYOUT_5POINT1|GENESIS_CH_BACK_LEFT|GENESIS_CH_BACK_RIGHT)
#define GENESIS_CH_LAYOUT_7POINT1_WIDE      (GENESIS_CH_LAYOUT_5POINT1|GENESIS_CH_FRONT_LEFT_OF_CENTER|GENESIS_CH_FRONT_RIGHT_OF_CENTER)
#define GENESIS_CH_LAYOUT_7POINT1_WIDE_BACK (GENESIS_CH_LAYOUT_5POINT1_BACK|GENESIS_CH_FRONT_LEFT_OF_CENTER|GENESIS_CH_FRONT_RIGHT_OF_CENTER)
#define GENESIS_CH_LAYOUT_OCTAGONAL         (GENESIS_CH_LAYOUT_5POINT0|GENESIS_CH_BACK_LEFT|GENESIS_CH_BACK_CENTER|GENESIS_CH_BACK_RIGHT)
#define GENESIS_CH_LAYOUT_STEREO_DOWNMIX    (GENESIS_CH_STEREO_LEFT|GENESIS_CH_STEREO_RIGHT)


/**
 * Return a channel layout id that matches name, or 0 if no match is found.
 *
 * name can be one or several of the following notations,
 * separated by '+' or '|':
 * - the name of an usual channel layout (mono, stereo, 4.0, quad, 5.0,
 *   5.0(side), 5.1, 5.1(side), 7.1, 7.1(wide), downmix);
 * - the name of a single channel (FL, FR, FC, LFE, BL, BR, FLC, FRC, BC,
 *   SL, SR, TC, TFL, TFC, TFR, TBL, TBC, TBR, DL, DR);
 * - a number of channels, in decimal, optionally followed by 'c', yielding
 *   the default channel layout for that number of channels (@see
 *   av_get_default_channel_layout);
 * - a channel layout mask, in hexadecimal starting with "0x" (see the
 *   AV_CH_* macros).
 *
 * Example: "stereo+FC" = "2+FC" = "2c+1c" = "0x7"
 */
uint64_t genesis_get_channel_layout(const char *name);

/* Return a description of a channel layout */
const char *genesis_get_channel_layout_string(uint64_t channel_layout);

/**
 * Return the number of channels in the channel layout.
 */
int genesis_get_channel_layout_nb_channels(uint64_t channel_layout);

/**
 * Return default channel layout for a given number of channels.
 */
uint64_t genesis_get_default_channel_layout(int nb_channels);

/**
 * Get the index of a channel in channel_layout.
 *
 * @param channel a channel layout describing exactly one channel which must be
 *                present in channel_layout.
 *
 * @return index of channel in channel_layout on success, a negative AVERROR
 *         on error.
 */
int genesis_get_channel_layout_channel_index(uint64_t channel_layout,
                                        uint64_t channel);

/**
 * Get the channel with the given index in channel_layout.
 */
uint64_t genesis_channel_layout_extract_channel(uint64_t channel_layout, int index);

/**
 * Get the name of a given channel.
 *
 * @return channel name on success, NULL on error.
 */
const char *genesis_get_channel_name(uint64_t channel);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif // GENESIS_CHANNEL_LAYOUT_H
