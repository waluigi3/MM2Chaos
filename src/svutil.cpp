#include "mmchaos/svutil.h"
#include <algorithm>

namespace mmchaos {
    namespace svutil {
        std::string_view split(std::string_view s, char c, sz& first) {
            std::string_view ret;
            auto second = s.find_first_of(c, first);

            if (second == std::string_view::npos) {
                ret = s.substr(first);
                second = s.size();
            } else {
                ret = s.substr(first,second - first);
            }

            first = second;

            return ret;
        }

        static bool is_space(char c) {
            switch (c) {
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                case '\v':
                case '\f':
                return true;
            }

            return false;
        }

        std::string_view strip(std::string_view s) {
            std::string_view ret;
            auto begin = std::find_if_not(s.begin(), s.end(), is_space);
            auto end = std::find_if_not(s.rbegin(), s.rend(), is_space);

            if (begin < end.base()) {
                return s.substr(begin - s.begin(), end.base() - begin);
            }

            return ret;
        }
    }
}