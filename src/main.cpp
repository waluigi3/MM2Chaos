#include "mmchaos/frame.h"
#include "mmchaos/game.h"
#include "nn/fs.h"

#include "mmchaos/types.h"
#include "mmchaos/input.h"
#include "mmchaos/bufmap.h"
#include "nn/hid.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <system_error>

/*
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
*/

namespace mmchaos {
    namespace main {
        #define COPY_TO_KEY(key, pos, x) std::memcpy(key + pos, &x, sizeof(x)); pos += sizeof(x)
        #define PARSE_ERR_RET(r) if (r.ec != std::errc()) {return;}

        constexpr sz IN_FILE_MAP_LEN = 37;
        constexpr sz IN_FILE_COMMAND_LEN = 4;
        constexpr sz IN_FILE_KEY_LEN = 8;
        constexpr sz IN_FILE_OUT_LEN = 6;
        const char* IN_FILE_NAME = "sd:/command.txt";
        const char* OUT_FILE_NAME = "sd:/out.log";

        template<typename... Args>
        using binary_key = std::array<uint8, (... + sizeof(Args))>;
        using block_key = binary_key<uint8, uint32, uint32, uint32>;

        struct block_update {
            uint32 newattribute;
        };

        struct timed_input {
            unsigned int frame;
            uint64 button;
            bool pressed;
        };

        struct exponential_delay {
            unsigned int start_frame;
            unsigned int delay;
            unsigned int min_delay;
            unsigned int max_delay;
        };

        enum run_state {
            WAITING_INPUT,
            WAITING_DELETE,
            RUNNING,
        };

        nn::fs::WriteOption write_option_empty = {0};
        int64 out_file_pos = 0;

        std::array<char, 32768> input_file;
        bufmap<block_key, block_update, 256> block_map;
        std::array<timed_input, 1024> timed_inputs;
        uint64 pressed_buttons = 0;
        int timed_input_len = 0;
        int timed_input_pos = 0;
        std::array<unsigned int, 16> output_frames;
        int output_frame_len = 0;
        int output_frame_pos = 0;
        unsigned int frame_offset = 0;

        run_state current_state = WAITING_INPUT;
        exponential_delay file_delay {.start_frame = 0, .delay = 0, .min_delay = 30, .max_delay = 480};
        bool perf = false;

        static void set_block_key(block_key& key, uint8 world, uint32 x, uint32 y, uint32 id) {
            sz pos = 0;
            COPY_TO_KEY(key.data(), pos, world);
            COPY_TO_KEY(key.data(), pos, x);
            COPY_TO_KEY(key.data(), pos, y);
            COPY_TO_KEY(key.data(), pos, id);
        }

        static void set_block_key(block_key& key, uint8 world, float x, float y, uint32 id) {
            sz pos = 0;
            COPY_TO_KEY(key.data(), pos, world);
            COPY_TO_KEY(key.data(), pos, x);
            COPY_TO_KEY(key.data(), pos, y);
            COPY_TO_KEY(key.data(), pos, id);
        }

        static sz parse_skip_newline(std::string_view sv, sz pos) {
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

        // Use fixed width format for line
        // 0=world, 1=subworld
        // | x pos    y pos    id       attributes
        // 1 40000000 40000000 FFFF0005 06000040
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

            set_block_key(key, world, x, y, id);
            block_update val {.newattribute = newattribute};

            block_map.set(key, val);
        }

        static void parse_input_line(std::string_view line, bool pressed) {
            unsigned int frame;
            uint64 button;

            if (timed_input_len >= timed_inputs.size()) {
                return;
            }

            auto res = std::from_chars(line.data(), line.data() + 6, frame);
            PARSE_ERR_RET(res);

            char bc = line[7];
            switch (bc) {
            case 'u':
                button = nn::hid::BUTTON_UP;
                break;
            case 'd':
                button = nn::hid::BUTTON_DOWN;
                break;
            case 'l':
                button = nn::hid::BUTTON_LEFT;
                break;
            case 'r':
                button = nn::hid::BUTTON_RIGHT;
                break;
            case '+':
                button = nn::hid::BUTTON_PLUS;
                break;
            case '-':
                button = nn::hid::BUTTON_MINUS;
                break;
            default:
                return;
            }

            timed_inputs[timed_input_len].button = button;
            timed_inputs[timed_input_len].frame = frame;
            timed_inputs[timed_input_len].pressed = pressed;
            timed_input_len++;
        }

        static void parse_out_line(std::string_view line) {
            unsigned int frame;

            if (output_frame_len >= output_frames.size()) {
                return;
            }

            auto res = std::from_chars(line.data(), line.data() + 6, frame);
            PARSE_ERR_RET(res);

            output_frames[output_frame_len] = frame;
            output_frame_len++;
        }

        static bool load_commands(const char* path) {
            nn::fs::FileHandle f;
            bool success = nn::fs::OpenFile(&f, path, nn::fs::MODE_READ) == 0;
            if (!success) {
                return false;
            }
            size_t read;
            success = nn::fs::ReadFile(&read, f, 0, input_file.data(), input_file.size()) == 0;
            nn::fs::CloseFile(f);
            if (!success) {
                return false;
            }
            std::string_view map_view(input_file.data(), read);

            block_map.clear();
            timed_input_len = 0;
            timed_input_pos = 0;
            output_frame_len = 0;
            output_frame_pos = 0;

            size_t pos = 0;
            while (read >= pos + IN_FILE_COMMAND_LEN) {
                std::string_view command(map_view.data() + pos, IN_FILE_COMMAND_LEN);
                pos += IN_FILE_COMMAND_LEN;
                if (command == "map " && read >= pos + IN_FILE_MAP_LEN) {
                    parse_block_map_line(std::string_view(map_view.data() + pos, IN_FILE_MAP_LEN));
                    pos += IN_FILE_MAP_LEN;
                    pos = parse_skip_newline(map_view, pos);
                } else if (command == "d   " && read >= pos + IN_FILE_KEY_LEN) {
                    parse_input_line(std::string_view(map_view.data() + pos, IN_FILE_KEY_LEN), true);
                    pos += IN_FILE_KEY_LEN;
                    pos = parse_skip_newline(map_view, pos);
                } else if (command == "u   " && read >= pos + IN_FILE_KEY_LEN) {
                    parse_input_line(std::string_view(map_view.data() + pos, IN_FILE_KEY_LEN), false);
                    pos += IN_FILE_KEY_LEN;
                    pos = parse_skip_newline(map_view, pos);
                } else if (command == "o   " && read >= pos + IN_FILE_OUT_LEN) {
                    parse_out_line(std::string_view(map_view.data() + pos, IN_FILE_OUT_LEN));
                    pos += IN_FILE_OUT_LEN;
                    pos = parse_skip_newline(map_view, pos);
                }
            }

            return true;
        }

        static void transform_block(int world, game::mm_block* b) {
            block_key key;
            set_block_key(key, world, b->x, b->y, b->id);
            auto mapping = block_map.get(key);
            if (mapping.has_value()) {
                b->attr1 = mapping->newattribute;
            }
        }

        static void write_output_line(nn::fs::FileHandle f, int x) {
            if (x > 9999) {
                x = 9999;
            }

            char buf[16];
            std::snprintf(buf, 16, "%04i\n", x);
            nn::fs::WriteFile(f, out_file_pos, buf, 5, write_option_empty);
            out_file_pos += 5;
        }

        // return true if done
        bool update_pressed(unsigned int frame) {
            auto old = pressed_buttons;
            auto pos = timed_input_pos;
            while (pos < timed_input_len) {
                if (timed_inputs[pos].frame > frame) {
                    break;
                }
                if (timed_inputs[pos].pressed) {
                    pressed_buttons |= timed_inputs[pos].button;
                } else {
                    pressed_buttons &= ~timed_inputs[pos].button;
                }
                pos++;
            }

            timed_input_pos = pos;
            
            if (old != pressed_buttons) {
                input::set_buttons_pressed(pressed_buttons);
            }

            return timed_input_pos == timed_input_len;
        }

        // return true if done
        bool update_output(unsigned int frame) {
            auto pos = output_frame_pos;
            bool out_open = false;
            nn::fs::FileHandle out_file;

            while (pos < output_frame_len) {
                if (output_frames[pos] > frame) {
                    break;
                }

                if (!out_open) {
                    if (nn::fs::OpenFile(&out_file, OUT_FILE_NAME, nn::fs::MODE_WRITE | nn::fs::MODE_APPEND) != 0) {
                        continue;
                    }
                    out_open = true;
                }
                
                int cc = game::get_clear_count();
                write_output_line(out_file, cc);
                pos++;
            }

            if (out_open) {
                nn::fs::FlushFile(out_file);
                nn::fs::CloseFile(out_file);
            }

            output_frame_pos = pos;

            return output_frame_pos == output_frame_len;
        }

        bool try_delay(exponential_delay& delay, unsigned int frame) {
            if (delay.delay == 0) { // always allow first call
                delay.delay = delay.min_delay;
                delay.start_frame = frame;
                return true;
            } else if (frame - delay.start_frame >= delay.delay) { // if delay elapsed
                delay.delay *= 2;
                if (delay.delay > delay.max_delay) {
                    delay.delay = delay.max_delay;
                }
                delay.start_frame = frame;
                return true;
            }

            return false; // not elapsed
        }

        void reset_delay(exponential_delay& delay) {
            delay.delay = 0;
        }

        bool try_load_file(const char* path, unsigned int frame, exponential_delay& delay) {
            if (try_delay(delay, frame) && load_commands(path)) {
                reset_delay(delay);
                return true;
            }
            return false;
        }

        bool try_delete_file(const char* path, unsigned int frame, exponential_delay& delay) {
            if (try_delay(delay, frame) && (nn::fs::DeleteFile(path) == 0)) {
                reset_delay(delay);
                return true;
            }
            return false;
        }
        
        void frame_cb(unsigned int frame) {
            if (current_state == WAITING_INPUT) {
                if (try_load_file(IN_FILE_NAME, frame, file_delay)) {
                    game::iterate_blocks(transform_block);
                    current_state = WAITING_DELETE;
                }
            }

            if (current_state == WAITING_DELETE) {
                if (try_delete_file(IN_FILE_NAME, frame, file_delay)) {
                    frame_offset = frame;
                    current_state = RUNNING;
                }
            }

            if (current_state == RUNNING) {
                frame -= frame_offset;
                bool input_done = update_pressed(frame);
                bool output_done = update_output(frame);
                if (input_done && output_done) {
                    current_state = WAITING_INPUT;
                }
            }
        }

        void input_cb(uint64 buttons_triggered) {
            if (buttons_triggered & nn::hid::BUTTON_STICK_L) {
                perf = !perf;
                game::set_performance_mode(perf);
            }
        }

        static void run() {
            nn::fs::MountSdCardForDebug("sd");

            input::init(input_cb);
            frame::init(frame_cb);
            game::init();
        }
    }
}

extern "C" void hkMain() {
    mmchaos::main::run();
}