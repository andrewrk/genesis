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
    GenesisErrorOpeningMidiHardware,
    GenesisErrorOpeningAudioHardware,
    GenesisErrorInvalidState,
    GenesisErrorSystemResources,
    GenesisErrorDecodingAudio,
    GenesisErrorInvalidParam,
    GenesisErrorInvalidPortType,
    GenesisErrorPortNotFound,
    GenesisErrorNoAudioFound,
    GenesisErrorUnimplemented,
    GenesisErrorAborted,
    GenesisErrorFileAccess,
    GenesisErrorInvalidFormat,
    GenesisErrorEmptyFile,
};

const char *genesis_error_string(int error);

#ifdef __cplusplus
}
#endif
#endif
