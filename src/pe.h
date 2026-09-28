#pragma once
#include <windows.h>
#include <cstdint>

namespace pe {

    inline bool is_valid(const uint8_t* data, size_t size) {
        if (size < sizeof(IMAGE_DOS_HEADER))
            return false;

        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(data);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return false;

        if (static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS) > size)
            return false;

        auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(data + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return false;

        if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
            return false;

        return true;
    }

    inline IMAGE_NT_HEADERS* nt_headers(uint8_t* data) {
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(data);
        return reinterpret_cast<IMAGE_NT_HEADERS*>(data + dos->e_lfanew);
    }

    inline const IMAGE_NT_HEADERS* nt_headers(const uint8_t* data) {
        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(data);
        return reinterpret_cast<const IMAGE_NT_HEADERS*>(data + dos->e_lfanew);
    }

    inline IMAGE_SECTION_HEADER* sections(uint8_t* data) {
        auto* nt = nt_headers(data);
        return IMAGE_FIRST_SECTION(nt);
    }

}
