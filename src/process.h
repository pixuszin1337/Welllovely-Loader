#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace process {

    struct Info {
        DWORD pid = 0;
        std::string name;
        std::string path;
        std::string client;
    };

    std::vector<Info> find_java();
    std::string detect_client(DWORD pid);
    HANDLE open(DWORD pid);

}
