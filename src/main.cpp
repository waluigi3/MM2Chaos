#include "hk/ro/RoUtil.h"
#include "hk/svc/api.h"
#include "hk/svc/cpu.h"
#include "nn/fs.h"
#include "hk/hook/Trampoline.h"

#include "mmchaos/input.h"
#include "mmchaos/bufmap.h"
#include "nn/hid.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cinttypes>
#include <string_view>
#include <system_error>

static nn::fs::FileHandle f1;
static std::array<std::uint8_t, 256> buf;
static int buf_pos = 0;
static int file_pos = 0;
static bool recording = false;

static HkTrampoline<bool, void*> pos_func = 
        hk::hook::trampoline([](void* t) -> bool {
            bool ret = pos_func.orig(t);

            if (!recording) {
                return ret;
            }

            std::uintptr_t vtable = *(std::uintptr_t*)t;

            // Get object address
            std::memcpy(&buf[buf_pos], &t, sizeof(t));
            buf_pos += sizeof(t);

            // Get vtable address (objectid)
            std::memcpy(&buf[buf_pos], &vtable, sizeof(vtable));
            buf_pos += sizeof(vtable);

            float* pos = (float*)(((uint8_t*)t)+0x230);
            std::memcpy(&buf[buf_pos], pos, sizeof(float)*3);
            buf_pos += sizeof(float)*3;

            nn::fs::WriteOption option = {.flags = nn::fs::WRITE_OPTION_FLUSH};
            if (buf_pos > 220) {
                nn::fs::WriteFile(f1, file_pos, buf.data(), buf_pos, option);
                file_pos += buf_pos;
                buf_pos = 0;
            }

            return ret;
        });

static void record_triggered() {
    if (recording) {
        nn::fs::WriteOption option = {.flags = nn::fs::WRITE_OPTION_FLUSH};
        nn::fs::WriteFile(f1, file_pos, buf.data(), buf_pos, option);
        nn::fs::CloseFile(f1);
    } else {
        char buf[64];
        std::uint64_t ticks = hk::svc::getSystemTick();
        std::snprintf(buf, 64, "sd:/%" PRIu64 ".bin", ticks);
        nn::fs::CreateFile(buf, 0);
        nn::fs::OpenFile(&f1, buf, nn::fs::MODE_WRITE | nn::fs::MODE_APPEND);
    }
    recording = !recording;
}

struct linked_list {
    linked_list* prev;
    linked_list* next;
};

struct mm_block {
    float x;
    float y;
    uint32_t un1;
    float sz_x;
    float sz_y;
    uint32_t attr1;
    uint32_t attr2;
    uint32_t extra;
    uint32_t id;
};

static void print_dbg(uint64_t x) {
    char buf[64];
    int len = std::snprintf(buf, 64, "%" PRIx64, x);
    hk::svc::OutputDebugString(buf, len);
}

using block_key = std::array<std::uint8_t, 9>;
void set_block_key(block_key& key, uint8_t world, uint32_t x, uint32_t y) {
    key[0] = world;
    std::memcpy(key.data() + 1, &x, sizeof(x));
    std::memcpy(key.data() + 1 + sizeof(x), &y, sizeof(y));
}

void set_block_key(block_key& key, uint8_t world, float x, float y) {
    key[0] = world;
    std::memcpy(key.data() + 1, &x, sizeof(x));
    std::memcpy(key.data() + 1 + sizeof(x), &y, sizeof(y));
}

struct block_update {
    uint32_t id;
    uint32_t newattribute;
};

// Use fixed width format for file
// 0=world, 1=subworld
// | x pos    y pos    id       attributes
// 1 40000000 40000000 FFFF0005 06000040
// struct.pack('>2f2I', 128.0, 64.0, 0xFFFF0005, 0x000000005).hex(' ', 4)
std::array<char, 16380> block_map_file;
mmchaos::bufmap<block_key, block_update, 256> block_map;
constexpr size_t MAP_FILE_LINE_LEN = 37;
#define PARSE_ERR_RET(r) if (r.ec != std::errc()) {return;}

static void parse_block_map_line(std::string_view line) {
    int world;
    uint32_t x , y, id, newattribute;
    block_key key;

    auto res = std::from_chars(line.data(), line.data() + 2, world);
    PARSE_ERR_RET(res);

    res = std::from_chars(line.data() + 2, line.data() + 11, x, 16);
    PARSE_ERR_RET(res);

    res = std::from_chars(line.data() + 11, line.data() + 20, y, 16);
    PARSE_ERR_RET(res);

    res = std::from_chars(line.data() + 20, line.data() + 29, id, 16);
    PARSE_ERR_RET(res);

    res = std::from_chars(line.data() + 29, line.data() + 37, newattribute, 16);
    PARSE_ERR_RET(res);

    set_block_key(key, world, x, y);
    block_update val {.id = id, .newattribute = newattribute};

    block_map.set(key, val);
}

static size_t parse_skip_newline(std::string_view sv, size_t pos) {
    if (sv.size() > pos) {
        char c = sv[pos];
        if (c == '\r' || c == '\n') {
            pos++;
        }
    }
    if (sv.size() > pos) {
        if (sv[pos] == '\n') {
            pos++;
        }
    }

    return pos;
}

static bool load_block_map(const char* path) {
    bool success;
    nn::fs::FileHandle f;
    if (nn::fs::OpenFile(&f, path, nn::fs::MODE_READ) != 0) {
        return false;
    }

    size_t read;
    success = nn::fs::ReadFile(&read, f, 0, block_map_file.data(), block_map_file.size()) == 0;
    nn::fs::CloseFile(f);
    if (!success) {
        return false;
    }
    std::string_view map_view(block_map_file.data(), read);

    block_map.clear();

    size_t pos = 0;
    while (read >= pos + MAP_FILE_LINE_LEN) {
        std::string_view line(map_view.data() + pos, MAP_FILE_LINE_LEN);
        parse_block_map_line(line);
        pos += MAP_FILE_LINE_LEN;
        pos = parse_skip_newline(map_view, pos);
    }

    return true;
}

static void transform_block(int world, mm_block* b) {
    block_key key;
    set_block_key(key, world, b->x, b->y);
    auto mapping = block_map.get(key);
    if (mapping.has_value() && mapping->id == b->id) {
        b->attr1 = mapping->newattribute;
    }
}

static void transform_triggered() {
    if (!load_block_map("sd:/blockmap.txt")) {
        return;
    }

    auto base = hk::ro::getMainModule()->getNnModule()->m_Base;
    uintptr_t ptr1 = base + 0x2B2B710;
    uintptr_t ptr2 = *(uintptr_t*)ptr1;
    linked_list* overworld_blocks = (linked_list*)(ptr2 + 0x20);
    linked_list* cur = overworld_blocks->next;
    
    while (cur != overworld_blocks) {
        mm_block* b = (mm_block*)(((uintptr_t)cur) - 40);
        transform_block(0, b);

        cur = cur->next;
    }

    linked_list* subworld_blocks = (linked_list*)(ptr2 + 0xC90);
    cur = subworld_blocks->next;
    while (cur != subworld_blocks) {
        mm_block* b = (mm_block*)(((uintptr_t)cur) - 56);
        transform_block(1, b);

        cur = cur->next;
    }
}

static int get_clear_count() {
    auto base = hk::ro::getMainModule()->getNnModule()->m_Base;
    uintptr_t ptr1 = base + 0x2A6D3A0;
    uintptr_t ptr2 = *(uintptr_t*)ptr1;
    
    return *(int*)(ptr2 + 0xA4);
}

static void get_cc_triggered() {
    bool success;
    nn::fs::FileHandle f;
    if (nn::fs::OpenFile(&f, "sd:/cc.txt", nn::fs::MODE_WRITE) != 0) {
        return;
    }

    int cc = get_clear_count();
    if (cc > 9999) {
        cc = 9999;
    }

    char buf[16];
    std::snprintf(buf, 16, "%04i", cc);

    nn::fs::WriteOption option = {.flags = nn::fs::WRITE_OPTION_FLUSH};
    nn::fs::WriteFile(f, 0, buf, 4, option);

    nn::fs::CloseFile(f);
}

static void input_cb() {
    if (mmchaos::input::is_multi_triggered(nn::hid::BUTTON_PLUS | nn::hid::BUTTON_LEFT)) {
        record_triggered();
    }
    if (mmchaos::input::is_multi_triggered(nn::hid::BUTTON_PLUS | nn::hid::BUTTON_RIGHT)) {
        transform_triggered();
    }
    if (mmchaos::input::is_multi_triggered(nn::hid::BUTTON_PLUS | nn::hid::BUTTON_DOWN)) {
        get_cc_triggered();
    }
}

//-------------- nvn hooks
static int callcount = 0;
static void (*nvnQueueSubmitCommands_orig)(void* obj, int commands, const void* handle) = nullptr;
static void nvnQueueSubmitCommands_hook(void* obj, int commands, const void* handle) {
    if (callcount % 4 != 0) {
        callcount++;
        return;
    }

    nvnQueueSubmitCommands_orig(obj, commands, handle);

    callcount++;
}


static void* (*nvnDeviceGetProcAddress_orig)(void*, const char*) = nullptr;
static void* nvnDeviceGetProcAddress_hook(void* device, const char* name) {
    void* orig = nvnDeviceGetProcAddress_orig(device, name);

    if (std::strcmp(name, "nvnQueueSubmitCommands") == 0) {
        nvnQueueSubmitCommands_orig = (void (*)(void*, int, const void*))orig;
        return (void*)nvnQueueSubmitCommands_hook;
    }

    return orig;
}

static HkTrampoline<void*, const char*> bootstrap_hook = hk::hook::trampoline([](const char* name) -> void* {
    void* orig = bootstrap_hook.orig(name);

    if (std::strcmp(name, "nvnDeviceGetProcAddress") == 0) {
        nvnDeviceGetProcAddress_orig = (void* (*)(void*, const char*))orig;
        return (void*)nvnDeviceGetProcAddress_hook;
    }

    return orig;
});

int drawcount = -100;
static HkTrampoline<void, void*> proc_draw_hook = hk::hook::trampoline([](void* obj) -> void {
    if (drawcount++ % 5 != 0 && drawcount > 0) {
        return;
    }
    proc_draw_hook.orig(obj);
});

static HkTrampoline<void, void*> waitforgpu_hook = hk::hook::trampoline([](void* obj) -> void {
    if (drawcount % 5 != 0 && drawcount > 0) {
        return;
    }
    waitforgpu_hook.orig(obj);
});

//static const std::array<rngmod::input::input_callback, 1> cb{rngmod::rng::input_press_callback};
static const std::array<mmchaos::input::input_callback, 1> cb{input_cb};
extern "C" void hkMain() {
    nn::fs::MountSdCardForDebug("sd");

    mmchaos::input::init(cb);

    pos_func.installAtSym<"actor_move">();

    bootstrap_hook.installAtSym<"nvnBootstrapLoader">();
    //proc_draw_hook.installAtSym<"proc_draw">();
    //waitforgpu_hook.installAtSym<"waitForGpuDone">();
}