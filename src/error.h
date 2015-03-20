#ifndef GENESIS_ERROR_H
#define GENESIS_ERROR_H

#ifdef __cplusplus
extern "C"
{
#endif

enum GenesisError {
    GenesisErrorNone,
    GenesisErrorNoMem,
    GenesisErrorMaxChannelsExceeded,
    GenesisErrorIncompatiblePorts,
    GenesisErrorInvalidPortDirection,
    GenesisErrorIncompatibleSampleRates,
    GenesisErrorIncompatibleChannelLayouts,
};

const char *genesis_error_string(enum GenesisError error);

#ifdef __cplusplus
}
#endif
#endif
