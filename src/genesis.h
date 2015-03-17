#ifndef GENESIS_GENESIS_H
#define GENESIS_GENESIS_H

#include "channel_layout.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GENESIS_VERSION_STRING "0.0.0"

struct GenesisContext;

enum GenesisChannelConfig {
    // there is a fixed channel layout that must be respected
    GenesisChannelConfigFixed,
    // any channel layout is supported
    GenesisChannelConfigAny,
    // any layout; the output layout is the same as the input layout
    GenesisChannelConfigSame,
};

enum GenesisPortType {
    GenesisPortTypeAudioIn,
    GenesisPortTypeAudioOut,
    GenesisPortTypeNotesIn,
    GenesisPortTypeNotesOut,
    GenesisPortTypeParamIn,
    GenesisPortTypeParamOut,
};

struct GenesisPortDescriptorAudioIn {
    struct GenesisChannelLayout channel_layout;
    enum GenesisChannelConfig channel_config;
};

struct GenesisPortDescriptorAudioOut {
    struct GenesisChannelLayout channel_layout;
    enum GenesisChannelConfig channel_config;
};

struct GenesisPortDescriptorNotesIn {
    int dummy;
};

struct GenesisPortDescriptorNotesOut {
    int dummy;
};

struct GenesisPortDescriptorParamIn {
    int dummy;
};

struct GenesisPortDescriptorParamOut {
    int dummy;
};

struct GenesisPortDescriptor {
    enum GenesisPortType port_type;
    const char *name;
    union {
        struct GenesisPortDescriptorAudioIn audio_in;
        struct GenesisPortDescriptorAudioOut audio_out;
        struct GenesisPortDescriptorNotesIn notes_in;
        struct GenesisPortDescriptorNotesOut notes_out;
        struct GenesisPortDescriptorParamIn param_in;
        struct GenesisPortDescriptorParamOut param_out;
    } data;
};

#define GENESIS_MAX_PORTS 32
struct GenesisNodeDescriptor {
    const char *name;
    int port_count;
    struct GenesisPortDescriptor port_descriptors[GENESIS_MAX_PORTS];
};

struct GenesisPort {
    enum GenesisPortType port_type;
    struct GenesisPortDescriptor *descriptor;
};

struct GenesisNode {
    const struct GenesisNodeDescriptor *descriptor;
    int port_count;
    struct GenesisPort ports[GENESIS_MAX_PORTS];
};

struct GenesisContext *genesis_create_context(void);
void genesis_destroy_context(struct GenesisContext *context);

struct GenesisNodeDescriptor *genesis_node_descriptor_find(
        struct GenesisContext *context, const char *name);


#ifdef __cplusplus
}
#endif
#endif
