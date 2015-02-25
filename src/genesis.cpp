#include "genesis.h"
#include "channel_layouts.hpp"
#include "audio_file.hpp"

void genesis_init(void) {
    genesis_init_channel_layouts();
    audio_file_init();
}
