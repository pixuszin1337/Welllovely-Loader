#include "console.h"
#include "process.h"
#include "mapper.h"
#include "stealth.h"
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

static std::filesystem::path get_exe_dir() {
    char buf[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
}

static std::vector<uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    if (sz <= 0) return {};
    f.seekg(0);
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

int main() {
    console::init();
    console::logo();

    console::blank();
    console::info("Loading DLL...");

    auto dll_path = get_exe_dir() / "WellLovely.dll";
    auto dll_data = read_file(dll_path);

    if (dll_data.empty()) {
        console::error("DLL not found: " + dll_path.string());
        console::wait_exit();
        return 1;
    }

    console::success("DLL loaded (" + std::to_string(dll_data.size() / 1024) + " KB)");

    console::blank();
    console::info("Scanning for Minecraft...");

    process::Info target{};
    bool found = false;

    for (int attempt = 0; attempt < 3; attempt++) {
        auto procs = process::find_java();

        if (procs.empty()) {
            if (attempt < 2) {
                console::warning("No Java process found. Retrying in 2s...");
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
            console::error("No Minecraft process found. Make sure the game is running.");
            console::wait_exit();
            return 1;
        }

        if (procs.size() == 1) {
            target = procs[0];
            found = true;
            break;
        }

        std::vector<std::string> options;
        for (auto& p : procs) {
            options.push_back(
                p.name + " (PID " + std::to_string(p.pid) + ") - " + p.client);
        }
        int choice = console::select("Multiple Java processes found. Select target:", options);
        target = procs[choice];
        found = true;
        break;
    }

    if (!found) return 1;

    console::success("Found: " + target.name + " (PID " + std::to_string(target.pid) + ")");
    console::success("Client: " + target.client);

    console::blank();
    console::info("Opening process...");
    HANDLE hProc = process::open(target.pid);
    if (!hProc) {
        console::error("Failed to open process. Try running as Administrator.");
        console::wait_exit();
        return 1;
    }
    console::success("Process opened");

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto result = mapper::manual_map(hProc, dll_data);
    CloseHandle(hProc);

    console::blank();
}
