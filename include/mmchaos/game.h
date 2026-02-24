#pragma once

#include "mmchaos/types.h"
#include "types.h"

namespace mmchaos {
    namespace game {
        struct linked_list {
            linked_list* prev;
            linked_list* next;
        };

        struct mm_block {
            float x;
            float y;
            uint32 un1;
            float sz_x;
            float sz_y;
            uint32 attr1;
            uint32 attr2;
            uint32 extra;
            uint32 id;
        };

        struct actor {
            char pad1[0x40];
            uint32 object_id;
            uint64 pad2;
            uint32 baseflags;
            char pad3[0x1DC];
            float x;
            float y;
            float z;
            char pad4[0xB4];
            uint32 flags;
        };

        struct enemyuber {
            actor actor;
            char pad1[0x208];
            float x_orig;
            float y_orig;
            float z_orig;
        };

        int get_clear_count();

        // int world, mm_block* block
        using block_callback = void(*)(int, mm_block*);
        void iterate_blocks(block_callback callback); 

        void init();
        void set_performance_mode(bool performance);
    }
}