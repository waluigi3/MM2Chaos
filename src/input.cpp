#include "mmchaos/input.h"

#include "nn/hid.h"
#include "hk/hook/Trampoline.h"

namespace mmchaos {
    namespace input {
        static std::span<const input_callback> callbacks;
        static uint64 last_buttons = 0;
        static uint64 triggered = 0;
        static HkTrampoline<int, nn::hid::full_key_state*, int, const uint32&> npad_func = 
        hk::hook::trampoline([](nn::hid::full_key_state* out, int count, const uint32& id) -> int {
            int written = npad_func.orig(out, count, id);

            if (written >= 1) {
                triggered = ~last_buttons & out[0].buttons;
                last_buttons = out[0].buttons;

                for(auto cb : callbacks) {
                    cb();
                }
            }

            return written;
        });

        void init(std::span<const input_callback> press_callbacks) {
            callbacks = press_callbacks;
            npad_func.installAtSym<"_ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj">();
        }

        bool is_triggered(uint64 button) {
            return (triggered & button) != 0;
        }

        bool is_multi_triggered(uint64 buttons) {
            bool anyTrigger = (triggered & buttons) != 0;
            bool allPressed = (last_buttons & buttons) == buttons;
            return anyTrigger && allPressed;
        }

        bool is_pressed(uint64 button) {
            return (last_buttons & button) != 0;
        }
    }
}