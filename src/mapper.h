#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>

namespace mapper {

    enum class Result {
        Success,
        InvalidPE,
        AllocFailed,
        WriteFailed,
        ShellcodeFailed,
        ThreadFailed,
        Timeout,
    };

    const char* to_string(Result r);
    Result manual_map(HANDLE hProcess, const std::vector<uint8_t>& dll_data);

}
