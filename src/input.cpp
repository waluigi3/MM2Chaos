#include "mmchaos/input.h"

#include "nn/hid.h"
#include "hk/hook/Trampoline.h"
#include "types.h"
#include <atomic>
#include <cstddef>

namespace mmchaos {
    namespace input {
        std::atomic<uint64> pressed{0};
        uint64 last_buttons = 0;
        input_callback input_cb;

        HkTrampoline<int, nn::hid::full_key_state*, int, const uint32&> npad_func = 
        hk::hook::trampoline([](nn::hid::full_key_state* out, int count, const uint32& id) -> int {
            int written = npad_func.orig(out, count, id);

            auto p = pressed.load();

            if (written >= 1) {
                uint64 triggered = ~last_buttons & out[0].buttons;
                if (triggered) {
                    input_cb(triggered);
                }
                last_buttons = out[0].buttons;
            }

            for (int i = 0; i < written; i++) {
                out[i].buttons |= p;
            }

            return written;
        });

        void init(input_callback callback) {
            input_cb = callback;
            npad_func.installAtSym<"_ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj">();
        }

        void set_buttons_pressed(uint64 buttons) {
            pressed.store(buttons);
        }
    }
}