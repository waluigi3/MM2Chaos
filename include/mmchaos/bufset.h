#pragma once

#include "types.h"

namespace mmchaos {
    template <class T, sz N>
    class bufset {
        T buf[N];
        sz pos = 0;
        bool all = false;

        public:
        void clear() {
            pos = 0;
            all = false;
        }

        void add(T t) {
            if (pos < N) {
                buf[pos] = t;
                pos++;
            }
        }

        void add_all() {
            all = true;
        }

        bool contains(T t) {
            if (all) {
                return true;
            }

            for (int i = 0; i < pos; i++) {
                if (buf[i] == t) {
                    return true;
                }
            }

            return false;
        }
    };
}