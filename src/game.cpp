#include "mmchaos/game.h"
#include "hk/hook/Trampoline.h"
#include "hk/ro/RoUtil.h"
#include <atomic>

namespace mmchaos {
    namespace game {
        int get_clear_count() {
            auto base = hk::ro::getMainModule()->getNnModule()->m_Base;
            uptr ptr1 = base + 0x2A6D3A0;
            uptr ptr2 = *(uptr*)ptr1;
            
            return *(int*)(ptr2 + 0xA4);
        }

        void iterate_blocks(block_callback callback) {
            auto base = hk::ro::getMainModule()->getNnModule()->m_Base;
            uptr ptr1 = base + 0x2B2B710;
            uptr ptr2 = *(uptr*)ptr1;
            linked_list* overworld_blocks = (linked_list*)(ptr2 + 0x20);
            linked_list* cur = overworld_blocks->next;
            
            while (cur != overworld_blocks) {
                mm_block* b = (mm_block*)(((uptr)cur) - 40);
                callback(0, b);

                cur = cur->next;
            }

            linked_list* subworld_blocks = (linked_list*)(ptr2 + 0xC90);
            cur = subworld_blocks->next;
            while (cur != subworld_blocks) {
                mm_block* b = (mm_block*)(((uptr)cur) - 56);
                callback(1, b);

                cur = cur->next;
            }
        }

        // for performance skip following functions
        std::atomic<bool> performance_mode{false};

        // very time consuming recursive text layout
        HkTrampoline<bool, void*, void*, void*> trySetLayoutMsg = 
            hk::hook::trampoline([](void* x, void* y, void* z) -> bool {
                if (performance_mode.load()) {
                    return false;
                }

                return trySetLayoutMsg.orig(x, y, z);
        });

        // rendering function
        HkTrampoline<void, void*, void*> drawRenderStep_ = 
            hk::hook::trampoline([](void* x, void* y) -> void {
                if (performance_mode.load()) {
                    return;
                }

                drawRenderStep_.orig(x, y);
        });

        // xlink function
        HkTrampoline<void, void*> xlink_calc = 
            hk::hook::trampoline([](void* x) -> void {
                if (performance_mode.load()) {
                    return;
                }

                xlink_calc.orig(x);
        });

        // model queue function (cant turn back on without glitches)
        HkTrampoline<void, void*, void*, unsigned int, int, int, int> pushBackToJobQueue_ = 
            hk::hook::trampoline([](void* p1, void* p2, unsigned int p3, int p4, int p5, int p6) -> void {
                if (performance_mode.load()) {
                    return;
                }

                pushBackToJobQueue_.orig(p1, p2, p3, p4, p5, p6);
        });

        // rendering function???
        HkTrampoline<void, void*, void*, int> calcLayerDL_ = 
            hk::hook::trampoline([](void* x, void* y, int z) -> void {
                if (performance_mode.load()) {
                    return;
                }

                calcLayerDL_.orig(x, y, z);
        });

        // rendering function???
        HkTrampoline<void, void*, void*, int> calcSubLayerDL_ = 
            hk::hook::trampoline([](void* x, void* y, int z) -> void {
                if (performance_mode.load()) {
                    return;
                }

                calcSubLayerDL_.orig(x, y, z);
        });

        // disable snapshot function to prevent crashes
        HkTrampoline<void, void*> SnapshotTextureMgr_update = 
            hk::hook::trampoline([](void* x) -> void {
                return;
        });

        void set_performance_mode(bool performance) {
            performance_mode.store(performance);
        }

        void init() {
            trySetLayoutMsg.installAtSym<"trySetLayoutMsg">();
            drawRenderStep_.installAtSym<"drawRenderStep_">();
            xlink_calc.installAtSym<"xlink_calc">();
            //pushBackToJobQueue_.installAtSym<"pushBackToJobQueue_">();

            //calcLayerDL_.installAtSym<"calcLayerDL_">();
            //calcSubLayerDL_.installAtSym<"calcSubLayerDL_">();
            SnapshotTextureMgr_update.installAtSym<"SnapshotTextureMgr_update">();
        }
    }
}