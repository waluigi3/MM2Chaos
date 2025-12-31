#pragma once

#include "../types.h"

namespace sead {
using namespace type_aliases;

    struct rng_context {
        uint32 X;
        uint32 Y;
        uint32 Z;
        uint32 W;
    };
}