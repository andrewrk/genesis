#include "error.h"
#include "util.hpp"

const char *genesis_error_string(enum GenesisError error) {
    switch (error) {
        case GenesisErrorNone: return "(no error)";
        case GenesisErrorNoMem: return "out of memory";
        case GenesisErrorMaxChannelsExceeded: return "max channels exceeded";
        case GenesisErrorIncompatiblePorts: return "incompatible ports";
        case GenesisErrorInvalidPortDirection: return "invalid port direction";
        case GenesisErrorIncompatibleSampleRates: return "incompatible sample rates";
        case GenesisErrorIncompatibleChannelLayouts: return "incompatible channel layouts";
    }
    panic("invalid error enum value");
}
