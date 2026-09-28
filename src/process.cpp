#include "process.h"
#include "stealth.h"
#include <tlhelp32.h>
#include <algorithm>

static std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), size, nullptr, nullptr);
    return result;
}

static std::string get_process_path(DWORD pid) {
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return {};

    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    QueryFullProcessImageNameW(hProc, 0, path, &size);
    CloseHandle(hProc);
    return wide_to_utf8(path);
}

std::vector<process::Info> process::find_java() {
    std::vector<Info> results;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return results;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring name(pe.szExeFile);
            std::wstring lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

            if (lower == L"javaw.exe" || lower == L"java.exe") {
                Info info;
                info.pid = pe.th32ProcessID;
                info.name = wide_to_utf8(name);
                info.path = get_process_path(pe.th32ProcessID);
                info.client = detect_client(pe.th32ProcessID);
                results.push_back(std::move(info));
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return results;
}

std::string process::detect_client(DWORD pid) {
    std::string path = get_process_path(pid);
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("lunar") != std::string::npos)
        return "Custom Client";

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        if (Module32FirstW(snap, &me)) {
            do {
                std::wstring modName(me.szModule);
                std::wstring modLower = modName;
                std::transform(modLower.begin(), modLower.end(), modLower.begin(), ::towlower);

                if (modLower.find(L"lunar") != std::wstring::npos) {
                    CloseHandle(snap);
                    return "Custom Client";
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
    }

    if (lower.find("forge") != std::string::npos)
        return "Forge";

    return "Minecraft";
}

HANDLE process::open(DWORD pid) {
    return OpenProcess(
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ |
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
}
