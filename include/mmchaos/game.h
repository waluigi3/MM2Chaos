#pragma once

#include "mmchaos/types.h"

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

        int get_clear_count();

        // int world, mm_block* block
        using block_callback = void(*)(int, mm_block*);
        void iterate_blocks(block_callback callback); 

        void init();
        void set_performance_mode(bool performance);
    }
}