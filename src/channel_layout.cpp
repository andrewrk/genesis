#include "channel_layout.h"
#include "util.hpp"

static GenesisChannelLayout builtin_channel_layouts[] = {
    {
        "Mono",
        1,
        {
            GenesisChannelIdFrontCenter,
        },
    },
    {
        "Stereo",
        2,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
        },
    },
    {
        "2.1",
        3,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdLowFrequency,
        },
    },
    {
        "3.0",
        3,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
        }
    },
    {
        "3.0 (back)",
        3,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdBackCenter,
        }
    },
    {
        "3.1",
        4,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "4.0",
        4,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdBackCenter,
        }
    },
    {
        "4.1",
        4,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "Quad",
        4,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdBackLeft,
            GenesisChannelIdBackRight,
        },
    },
    {
        "Quad (side)",
        4,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
        }
    },
    {
        "5.0",
        5,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
        }
    },
    {
        "5.0 (back)",
        5,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdBackLeft,
            GenesisChannelIdBackRight,
        }
    },
    {
        "5.1",
        6,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "5.1 (back)",
        6,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdBackLeft,
            GenesisChannelIdBackRight,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "6.0",
        6,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdBackCenter,
        }
    },
    {
        "6.0 (front)",
        6,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdFrontLeftOfCenter,
            GenesisChannelIdFrontRightOfCenter,
        }
    },
    {
        "Hexagonal",
        6,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdBackLeft,
            GenesisChannelIdBackRight,
            GenesisChannelIdBackCenter,
        }
    },
    {
        "6.1",
        7,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdBackCenter,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "6.1 (back)",
        7,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdBackLeft,
            GenesisChannelIdBackRight,
            GenesisChannelIdBackCenter,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "6.1 (front)",
        7,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdFrontLeftOfCenter,
            GenesisChannelIdFrontRightOfCenter,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "7.0",
        7,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdBackLeft,
            GenesisChannelIdBackRight,
        }
    },
    {
        "7.0 (front)",
        7,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdFrontLeftOfCenter,
            GenesisChannelIdFrontRightOfCenter,
        }
    },
    {
        "7.1",
        8,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdBackLeft,
            GenesisChannelIdBackRight,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "7.1 (wide)",
        8,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdFrontLeftOfCenter,
            GenesisChannelIdFrontRightOfCenter,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "7.1 (wide) (back)",
        8,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdBackLeft,
            GenesisChannelIdBackRight,
            GenesisChannelIdFrontLeftOfCenter,
            GenesisChannelIdFrontRightOfCenter,
            GenesisChannelIdLowFrequency,
        }
    },
    {
        "Octagonal",
        8,
        {
            GenesisChannelIdFrontLeft,
            GenesisChannelIdFrontRight,
            GenesisChannelIdFrontCenter,
            GenesisChannelIdSideLeft,
            GenesisChannelIdSideRight,
            GenesisChannelIdBackLeft,
            GenesisChannelIdBackRight,
            GenesisChannelIdBackCenter,
        }
    },
};

bool genesis_channel_layout_equal(const GenesisChannelLayout *a, const GenesisChannelLayout *b) {
    if (a->channel_count != b->channel_count)
        return false;

    for (int i = 0; i < a->channel_count; i += 1) {
        if (a->channels[i] != b->channels[i])
            return false;
    }

    return true;
}

const char *genesis_get_channel_name(enum GenesisChannelId id) {
    switch (id) {
    case GenesisChannelIdInvalid: return "(Invalid Channel)";
    case GenesisChannelIdFrontLeft: return "Front Left";
    case GenesisChannelIdFrontRight: return "Front Right";
    case GenesisChannelIdFrontCenter: return "Front Center";
    case GenesisChannelIdLowFrequency: return "Low Frequency";
    case GenesisChannelIdBackLeft:  return "Back Left";
    case GenesisChannelIdBackRight: return "Back Right";
    case GenesisChannelIdFrontLeftOfCenter: return "Front Left of Center";
    case GenesisChannelIdFrontRightOfCenter: return "Front Right of Center";
    case GenesisChannelIdBackCenter: return "Back Center";
    case GenesisChannelIdSideLeft: return "Side Left";
    case GenesisChannelIdSideRight: return "Side Right";
    case GenesisChannelIdTopCenter: return "Top Center";
    case GenesisChannelIdTopFrontLeft: return "Top Front Left";
    case GenesisChannelIdTopFrontCenter: return "Top Front Center";
    case GenesisChannelIdTopFrontRight: return "Top Front Right";
    case GenesisChannelIdTopBackLeft: return "Top Back Left";
    case GenesisChannelIdTopBackCenter: return "Top Back Center";
    case GenesisChannelIdTopBackRight: return "Top Back Right";
    }
    panic("invalid channel id");
}

int genesis_channel_layout_builtin_count(void) {
    return array_length(builtin_channel_layouts);
}

const GenesisChannelLayout *genesis_channel_layout_get_builtin(int index) {
    if (index < 0 || index >= array_length(builtin_channel_layouts))
        panic("channel layout id out of bounds");
    return &builtin_channel_layouts[index];
}
