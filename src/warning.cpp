#include "warning.hpp"
#include "util.hpp"
#include <stdio.h>

static bool seen_warnings[WarningCount] = {0};

void emit_warning(Warning warning) {
    if (seen_warnings[warning])
        return;
    seen_warnings[warning] = true;

    switch (warning) {
        case WarningHighPriorityThread:
            fprintf(stderr, "warning: unable to set high priority thread: Operation not permitted\n");
            fprintf(stderr, "See https://github.com/andrewrk/genesis/wiki/warning:-unable-to-set-high-priority-thread:-Operation-not-permitted\n");
            return;
        case WarningCount:
            panic("invalid warning");
    }
    panic("invalid warning");

}
