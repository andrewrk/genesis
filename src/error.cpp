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
        case GenesisErrorInvalidState: return "invalid state";
        case GenesisErrorSystemResources: return "system resource not available";
        case GenesisErrorDecodingAudio: return "decoding audio";
    }
    panic("invalid error enum value");
}
