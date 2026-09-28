#pragma once
#include <string>
#include <cstddef>

namespace xor_detail {

    template<size_t N>
    struct encrypted_string {
        char data[N]{};
        static constexpr char KEY = 0x7A;

        constexpr encrypted_string(const char(&str)[N]) {
            for (size_t i = 0; i < N; i++)
                data[i] = str[i] ^ static_cast<char>(KEY + (i & 0x1F));
        }

        std::string decrypt() const {
            std::string result;
            result.reserve(N - 1);
            for (size_t i = 0; i < N - 1; i++)
                result += static_cast<char>(data[i] ^ static_cast<char>(KEY + (i & 0x1F)));
            return result;
        }
    };

}

#define xs(s) (::xor_detail::encrypted_string(s).decrypt())
#define xws(s) ([]{ auto d = ::xor_detail::encrypted_string(s).decrypt(); \
    return std::wstring(d.begin(), d.end()); }())
