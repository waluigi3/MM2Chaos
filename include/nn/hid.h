#pragma once

#include "types.h"

namespace nn {
    namespace hid {
        constexpr uint64 BUTTON_STICK_L = 0x10;
        constexpr uint64 BUTTON_STICK_R = 0x20;
        constexpr uint64 BUTTON_MINUS = 0x800;
        constexpr uint64 BUTTON_PLUS = 0x400;
        constexpr uint64 BUTTON_LEFT = 0x1000;
        constexpr uint64 BUTTON_UP = 0x2000;
        constexpr uint64 BUTTON_RIGHT = 0x4000;
        constexpr uint64 BUTTON_DOWN = 0x8000;

        struct full_key_state {
            int64 sample;
            uint64 buttons;
            int32 sl_x;
            int32 sl_y;
            int32 sr_x;
            int32 sr_y;
            uint32 attributes;
        };
    }
}