#include "mmchaos/frame.h"
#include "hk/hook/Trampoline.h"

namespace mmchaos {
    namespace frame {
        unsigned int frame = 0;
        frame_callback frame_cb;

        HkTrampoline<void, void*> procFrame_ = 
            hk::hook::trampoline([](void* t) -> void {
                procFrame_.orig(t);
                frame_cb(frame++);
        });

        void init(frame_callback callback) {
            frame_cb = callback;
            procFrame_.installAtSym<"procFrame_">();
        }
    }
}