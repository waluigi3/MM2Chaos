#pragma once

#include "types.h"

namespace mmchaos {
    namespace logger {
        constexpr sz BUF_SIZE = 4096;

        struct log_ctx {
            char buf[BUF_SIZE];
            sz pos;
            sz file_pos;
            const char* filename;
        };

        void log_init(log_ctx& ctx, const char* filename);
        void log_flush(log_ctx& ctx);
        void log_write(log_ctx& ctx, const char* data, sz len);
    }
}