#pragma once

#include "types.h"

namespace nn {
    namespace os {
        struct MutexType {
            uint8 x1;
            bool x2;
            uint32 x3;
            uint32 x4;
            void* x5;
            uint32 x6;
        };

        void InitializeMutex(MutexType* mutex, bool unk, int level);
        void LockMutex(MutexType* mutex);
        void UnlockMutex(MutexType* mutex);
    }
}