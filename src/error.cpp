#include "error.h"
#include "util.hpp"

const char *genesis_error_string(int error) {
    switch ((GenesisError)error) {
        case GenesisErrorNone: return "(no error)";
        case GenesisErrorNoMem: return "out of memory";
        case GenesisErrorMaxChannelsExceeded: return "max channels exceeded";
        case GenesisErrorIncompatiblePorts: return "incompatible ports";
        case GenesisErrorInvalidPortDirection: return "invalid port direction";
        case GenesisErrorIncompatibleSampleRates: return "incompatible sample rates";
        case GenesisErrorIncompatibleChannelLayouts: return "incompatible channel layouts";
        case GenesisErrorOpeningMidiHardware: return "opening midi hardware";
        case GenesisErrorOpeningAudioHardware: return "opening audio hardware";
        case GenesisErrorInvalidState: return "invalid state";
        case GenesisErrorSystemResources: return "system resource not available";
        case GenesisErrorDecodingAudio: return "decoding audio";
        case GenesisErrorInvalidParam: return "invalid parameter";
        case GenesisErrorInvalidPortType: return "invalid port type";
        case GenesisErrorPortNotFound: return "port not found";
        case GenesisErrorNoAudioFound: return "no audio found";
        case GenesisErrorUnimplemented: return "unimplemented (patch welcome!)";
        case GenesisErrorAborted: return "aborted";
        case GenesisErrorFileAccess: return "file access";
        case GenesisErrorInvalidFormat: return "invalid format";
        case GenesisErrorEmptyFile: return "empty file";
        case GenesisErrorKeyNotFound: return "key not found";
        case GenesisErrorFileNotFound: return "file not found";
        case GenesisErrorPermissionDenied: return "permission denied";
        case GenesisErrorNotDir: return "not a directory";
    }
    panic("invalid error enum value");
}
