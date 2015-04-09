#ifndef PROJECT_HPP
#define PROJECT_HPP

#include "genesis.h"
#include "hash_map.hpp"

struct AudioAsset {
    uint256 id;
    ByteBuffer path;
    GenesisAudioFile *audio_file;
};

struct AudioClip {
    uint256 id;
    AudioAsset *audio_asset;
    GenesisNode *node;
};

struct AudioClipSegment {
    uint256 id;
    AudioClip *audio_clip;
    long start;
    long end;
};

struct Project {
    IdMap<AudioClipSegment *> audio_clip_segments;
    IdMap<AudioClip *> audio_clips;
    IdMap<AudioASsets *> audio_assets;
};

#endif
