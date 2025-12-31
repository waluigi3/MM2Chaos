#pragma once

#include "types.h"

namespace nn {
    namespace fs {
        struct FileHandle {
            void* handle;
        };

        constexpr int MODE_READ = 1;
        constexpr int MODE_WRITE = 2;
        constexpr int MODE_APPEND = 4;

        struct WriteOption {
            int flags;
        };

        constexpr int WRITE_OPTION_FLUSH = 1;

        uint32 MountSdCardForDebug(const char* mount);

        uint32 CreateFile(const char* path, int64 length);
        uint32 OpenFile(FileHandle* handle, const char* path, int mode);
        uint32 ReadFile(sz* bytes_read, FileHandle handle, int64 off, void* data, sz bytes_to_read);
        uint32 WriteFile(FileHandle handle, int64 off, const void* data, sz bytes_to_write, const WriteOption& option);
        void CloseFile(FileHandle handle);
    }
}