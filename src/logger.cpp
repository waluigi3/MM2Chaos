#include "mmchaos/logger.h"

#include "nn/fs.h"
#include <cstring>

namespace mmchaos {
    namespace logger {
        nn::fs::WriteOption WO_EMPTY = {0};
        
        void log_init(log_ctx& ctx, const char* filename) {
            ctx.pos = 0;
            ctx.file_pos = 0;
            ctx.filename = filename;
        }

        void log_write_raw(log_ctx& ctx, const char* data, sz len) {
            nn::fs::FileHandle f;
            if (nn::fs::OpenFile(&f, ctx.filename, nn::fs::MODE_WRITE | nn::fs::MODE_APPEND) != 0) {
                return;
            }

            nn::fs::WriteFile(f, ctx.file_pos, data, len, WO_EMPTY);
            ctx.file_pos += len;

            nn::fs::FlushFile(f);
            nn::fs::CloseFile(f);
        }

        void log_flush(log_ctx& ctx) {
            if (ctx.pos > 0) {
                log_write_raw(ctx, ctx.buf, ctx.pos);
                ctx.pos = 0;
            }
        }

        void log_write(log_ctx& ctx, const char* data, sz len) {
            const char* current = data;

            while (len > 0) {
                sz space = BUF_SIZE - ctx.pos;

                // Ensure space
                if (space == 0) {
                    log_flush(ctx);
                    space = BUF_SIZE;
                }

                // Skip buffering if input larger than buffer
                if (len > BUF_SIZE) {
                    log_flush(ctx);
                    log_write_raw(ctx, data, len);
                    return;
                }

                sz copy = (len < space) ? len : space;
                std::memcpy(ctx.buf + ctx.pos, current, copy);

                ctx.pos += copy;
                current += copy;
                len -= copy;
            }
        }
    }
}