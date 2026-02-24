#include "hk/hook/Trampoline.h"
#include "hk/svc/api.h"
#include "mmchaos/frame.h"
#include "mmchaos/game.h"
#include "mmchaos/logger.h"
#include "mmchaos/bufset.h"


#include "mmchaos/types.h"
#include "mmchaos/input.h"
#include "mmchaos/bufmap.h"
#include "nn/hid.h"
#include "nn/fs.h"
#include "types.h"

#include <array>
#include <charconv>
#include <cinttypes>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <system_error>

namespace mmchaos {
    namespace main {
        #define COPY_TO_KEY(key, pos, x) std::memcpy(key + pos, &x, sizeof(x)); pos += sizeof(x)
        #define PARSE_ERR_RET(r) if (r.ec != std::errc()) {return;}

        constexpr sz IN_FILE_MAP_LEN = 37;
        constexpr sz IN_FILE_COMMAND_LEN = 4;
        constexpr sz IN_FILE_KEY_LEN = 8;
        constexpr sz IN_FILE_OUT_LEN = 6;
        constexpr sz IN_FILE_FLUSH_LEN = 6;
        constexpr sz IN_FILE_SET_LEN = 4;
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

        enum timed_output_type {
            OUTPUT_CLEAR_COUNT,
            OUTPUT_SYNC
        };

        struct timed_output {
            unsigned int frame;
            timed_output_type type;
        };

        enum set_type {
            SET_SPAWN,
            SET_DIE
        };

        std::array<char, 32768> input_file;
        bufmap<block_key, block_update, 256> block_map;
        std::array<timed_input, 1024> timed_inputs;
        uint64 pressed_buttons = 0;
        int timed_input_len = 0;
        int timed_input_pos = 0;
        std::array<timed_output, 16> output_frames;
        int output_frame_len = 0;
        int output_frame_pos = 0;
        unsigned int frame_offset = 0;
        logger::log_ctx out_log;
        bufset<uint32, 16> spawn_id_set;
        bufset<uint32, 16> die_id_set;

        run_state current_state = WAITING_INPUT;
        exponential_delay file_delay {.start_frame = 0, .delay = 0, .min_delay = 30, .max_delay = 480};
        bool perf = true;
        uint32 flush_interval = 0; // in frames
        uint32 flush_counter = 0;
        bool game_loaded = false;

        static void log_flush_timer() {
            if (flush_counter >= flush_interval) {
                flush_counter = 0;
                logger::log_flush(out_log);
            } else if (flush_interval > 0) {
                flush_counter++;
            }
        }

        // don't write 512+ chars or it will fail
        static void log(const char* fmt, ...) {
            constexpr sz BUF_LEN = 512;
            char buf[BUF_LEN];
            std::va_list args;
            va_start(args, fmt);

            int len = std::vsnprintf(buf, BUF_LEN, fmt, args);
            if (len > 0 && len < BUF_LEN) {
                logger::log_write(out_log, buf, len);
            }

            va_end(args);
        }

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
            case 'a':
                button = nn::hid::BUTTON_A;
                break;
            case 'b':
                button = nn::hid::BUTTON_B;
                break;
            case 'x':
                button = nn::hid::BUTTON_X;
                break;
            case 'y':
                button = nn::hid::BUTTON_Y;
                break;
            case '[':
                button = nn::hid::BUTTON_ZL;
                break;
            case ']':
                button = nn::hid::BUTTON_ZR;
                break;
            default:
                return;
            }

            timed_inputs[timed_input_len].button = button;
            timed_inputs[timed_input_len].frame = frame;
            timed_inputs[timed_input_len].pressed = pressed;
            timed_input_len++;
        }

        static void parse_out_line(std::string_view line, timed_output_type out_type) {
            unsigned int frame;

            if (output_frame_len >= output_frames.size()) {
                return;
            }

            auto res = std::from_chars(line.data(), line.data() + 6, frame);
            PARSE_ERR_RET(res);

            output_frames[output_frame_len].type = out_type;
            output_frames[output_frame_len].frame = frame;
            output_frame_len++;
        }

        static void parse_set_line(std::string_view line, set_type set_type) {
            uint32 object_id;
            auto res = std::from_chars(line.data(), line.data() + IN_FILE_SET_LEN, object_id);
            PARSE_ERR_RET(res);

            if (set_type == SET_SPAWN) {
                if (object_id == 9999) {
                    spawn_id_set.add_all();
                } else {
                    spawn_id_set.add(object_id);
                }
            } else if (set_type == SET_DIE) {
                if (object_id == 9999) {
                    die_id_set.add_all();
                } else {
                    die_id_set.add(object_id);
                }
            }
        }

        static void parse_flush_line(std::string_view line) {
            unsigned int interval;

            auto res = std::from_chars(line.data(), line.data() + IN_FILE_FLUSH_LEN, interval);
            PARSE_ERR_RET(res);

            flush_counter = 0;
            flush_interval = interval;
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
            spawn_id_set.clear();
            die_id_set.clear();

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
                    parse_out_line(std::string_view(map_view.data() + pos, IN_FILE_OUT_LEN), OUTPUT_CLEAR_COUNT);
                    pos += IN_FILE_OUT_LEN;
                    pos = parse_skip_newline(map_view, pos);
                } else if (command == "s   " && read >= pos + IN_FILE_OUT_LEN) {
                    parse_out_line(std::string_view(map_view.data() + pos, IN_FILE_OUT_LEN), OUTPUT_SYNC);
                    pos += IN_FILE_OUT_LEN;
                    pos = parse_skip_newline(map_view, pos);
                } else if (command == "as  " && read >= pos + IN_FILE_SET_LEN) {
                    parse_set_line(std::string_view(map_view.data() + pos, IN_FILE_SET_LEN), SET_SPAWN);
                    pos += IN_FILE_SET_LEN;
                    pos = parse_skip_newline(map_view, pos);
                } else if (command == "ad  " && read >= pos + IN_FILE_SET_LEN) {
                    parse_set_line(std::string_view(map_view.data() + pos, IN_FILE_SET_LEN), SET_DIE);
                    pos += IN_FILE_SET_LEN;
                    pos = parse_skip_newline(map_view, pos);
                } else if (command == "fi  " && read >= pos + IN_FILE_FLUSH_LEN) {
                    parse_flush_line(std::string_view(map_view.data() + pos, IN_FILE_FLUSH_LEN));
                    pos += IN_FILE_FLUSH_LEN;
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

            while (pos < output_frame_len) {
                timed_output& current = output_frames[pos];
                if (current.frame > frame) {
                    break;
                }
                
                if (current.type == OUTPUT_CLEAR_COUNT) {
                    int cc = game::get_clear_count();
                    cc = (cc > 9999) ? 9999 : cc;
                    log("%04i\n", cc);
                } else if (current.type == OUTPUT_SYNC) {
                    log("SYNC\n");
                    logger::log_flush(out_log);
                }
                
                pos++;
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

            log_flush_timer();
        }

        void input_cb(uint64 buttons_triggered) {
            if (buttons_triggered & nn::hid::BUTTON_STICK_L) {
                perf = !perf;
                game::set_performance_mode(perf);
            }
        }

        // following hooks should occur on same thread so no syncronization needed

        // hook to output actor spawns
        HkTrampoline<void, game::actor*, void*> actor_init = 
            hk::hook::trampoline([](game::actor* t, void* u) -> void {
                actor_init.orig(t, u);

                if (spawn_id_set.contains(t->object_id)) {
                    log("C|%" PRIu32 "|%.0f|%.0f\n", t->object_id, t->x, t->y);
                }

                if (!game_loaded) {
                    game_loaded = true;
                    log("LOAD\n");
                    logger::log_flush(out_log);
                }
        });

        constexpr uint32 DEAD_FLAG = 0x100;
        // hook to output "enemy" deaths
        HkTrampoline<void, game::enemyuber*> enemyuber_status_update = 
            hk::hook::trampoline([](game::enemyuber* t) -> void {
                uint32 old_dead_flag = t->actor.flags & DEAD_FLAG;
                enemyuber_status_update.orig(t);
                uint32 dead_flag = t->actor.flags & DEAD_FLAG;

                if (die_id_set.contains(t->actor.object_id) && old_dead_flag == 0 && dead_flag != 0) {
                    log("D|%" PRIu32 "|%.0f|%.0f|%.0f|%.0f\n", t->actor.object_id, t->actor.x, t->actor.y, t->x_orig, t->y_orig);
                }
        });

        static void run() {
            nn::fs::MountSdCardForDebug("sd");

            logger::log_init(out_log, OUT_FILE_NAME);

            input::init(input_cb);
            frame::init(frame_cb);
            game::init();

            actor_init.installAtSym<"actor_init">();
            enemyuber_status_update.installAtSym<"enemyuber_status_update">();

            hk::svc::OutputDebugString("CHAOS LOADED", 12);
        }
    }
}

extern "C" void hkMain() {
    mmchaos::main::run();
}