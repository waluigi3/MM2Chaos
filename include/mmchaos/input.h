#pragma once

#include "mmchaos/types.h"

namespace mmchaos {
    namespace input {
        using input_callback = void(*)(uint64);

        void init(input_callback callback);
        void set_buttons_pressed(uint64 buttons);
    }
}