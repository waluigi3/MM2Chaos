#pragma once

namespace mmchaos {
    namespace frame {
        using frame_callback = void(*)(unsigned int);

        void init(frame_callback callback);
    }
}