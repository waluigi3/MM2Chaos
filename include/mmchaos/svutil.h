#pragma once

#include "mmchaos/types.h"
#include <string_view>

namespace mmchaos {
    namespace svutil {
        std::string_view split(std::string_view s, char c, sz& first);
        std::string_view strip(std::string_view s);
    }
}