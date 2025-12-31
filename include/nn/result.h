#pragma once

#include "types.h"

namespace nn {
    namespace result {
        constexpr uint32 GetModule(uint32 res) {
            return res & 0x1FF;
        }

        constexpr uint32 GetDescription(uint32 res) {
            return (res >> 9) & 0x1FFF;
        }

        constexpr bool CheckResultRange(uint32 res, uint32 module, uint32 start, uint32 end) {
            uint32 desc = GetDescription(res);
            return GetModule(res) == res && desc >= start && desc < end;
        }
    }
}