#ifndef DEBUG_HPP
#define DEBUG_HPP

#include "util.hpp"
#include "genesis.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static inline void ok_or_panic(int err) {
    if (err)
        panic("%s", genesis_error_string(err));
}

#ifdef NDEBUG
static const bool GENESIS_DEBUG_MODE = false;
#else
static const bool GENESIS_DEBUG_MODE = true;
#endif

#define BREAKPOINT __asm("int $0x03")

#endif
