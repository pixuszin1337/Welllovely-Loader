#include "console.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <conio.h>

namespace {
    const char* PINK    = "\033[38;2;243;139;170m";
    const char* WHITE   = "\033[97m";
    const char* GRAY    = "\033[90m";
    const char* GREEN   = "\033[38;2;166;227;161m";
    const char* RED     = "\033[38;2;243;139;139m";
    const char* YELLOW  = "\033[38;2;249;226;175m";
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* DIM     = "\033[2m";
}

void console::init() {
    SetConsoleTitleA("WellLovely Loader");
    SetConsoleOutputCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    CONSOLE_CURSOR_INFO ci{};
    ci.dwSize = 1;
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &ci);

    system("cls");
}

void console::logo() {
    std::cout << "\n";
    std::cout << PINK << "  ════════════════════════════════════════" << RESET << "\n";
    std::cout << PINK << BOLD
              << "    ╦ ╦╔═╗╦  ╦  ╦  ╔═╗╦  ╦╔═╗╦  ╦ ╦" << RESET << "\n";
    std::cout << PINK << BOLD
              << "    ║║║║╣ ║  ║  ║  ║ ║╚╗╔╝║╣ ║  ╚╦╝" << RESET << "\n";
    std::cout << PINK << BOLD
              << "    ╚╩╝╚═╝╩═╝╩═╝╩═╝╚═╝ ╚╝ ╚═╝╩═╝ ╩" << RESET << "\n";
    std::cout << GRAY << "              Loader v1.0" << RESET << "\n";
    std::cout << PINK << "  ════════════════════════════════════════" << RESET << "\n";
    std::cout << "\n";
}

void console::separator() {
    std::cout << GRAY << "  ────────────────────────────────────────" << RESET << "\n";
}

void console::blank() {
    std::cout << "\n";
}

void console::info(const std::string& msg) {
    std::cout << "  " << PINK << "[*]" << RESET << " " << WHITE << msg << RESET << "\n";
}

void console::success(const std::string& msg) {
    std::cout << "  " << GREEN << "[+]" << RESET << " " << GREEN << msg << RESET << "\n";
}

void console::error(const std::string& msg) {
    std::cout << "  " << RED << "[-]" << RESET << " " << RED << msg << RESET << "\n";
}

void console::warning(const std::string& msg) {
    std::cout << "  " << YELLOW << "[!]" << RESET << " " << YELLOW << msg << RESET << "\n";
}

void console::step(const std::string& msg) {
    std::cout << "  " << DIM << GRAY << " >  " << msg << RESET << "\n";
}

void console::progress(const std::string& label, int current, int total) {
    const int bar_width = 30;
    float pct = static_cast<float>(current) / static_cast<float>(total);
    int filled = static_cast<int>(pct * bar_width);

    std::cout << "\r  " << PINK << "[*]" << RESET << " " << WHITE << label << " " << PINK;
    for (int i = 0; i < bar_width; i++)
        std::cout << (i < filled ? "\xe2\x96\x88" : "\xe2\x96\x91");
    std::cout << RESET << " " << WHITE << static_cast<int>(pct * 100) << "%" << RESET;

    if (current >= total)
        std::cout << "\n";
    else
        std::cout << std::flush;
}

int console::select(const std::string& prompt, const std::vector<std::string>& options) {
    std::cout << "\n";
    info(prompt);
    for (size_t i = 0; i < options.size(); i++) {
        std::cout << "  " << GRAY << "    [" << WHITE << i + 1 << GRAY << "] "
                  << WHITE << options[i] << RESET << "\n";
    }
    std::cout << "\n  " << PINK << " >  " << RESET << WHITE;

    int choice = 0;
    while (choice < 1 || choice > static_cast<int>(options.size())) {
        char c = static_cast<char>(_getch());
        int val = c - '0';
        if (val >= 1 && val <= static_cast<int>(options.size())) {
            choice = val;
            std::cout << choice << RESET << "\n";
        }
    }
    return choice - 1;
}

std::string console::input(const std::string& prompt) {
    info(prompt);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci{};
    ci.dwSize = 25;
    ci.bVisible = TRUE;
    SetConsoleCursorInfo(hOut, &ci);

    std::cout << "  " << PINK << " >  " << RESET << WHITE;
    std::string line;
    std::getline(std::cin, line);
    std::cout << RESET;

    ci.bVisible = FALSE;
    ci.dwSize = 1;
    SetConsoleCursorInfo(hOut, &ci);

    return line;
}

std::string console::input_masked(const std::string& prompt) {
    info(prompt);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci{};
    ci.dwSize = 25;
    ci.bVisible = TRUE;
    SetConsoleCursorInfo(hOut, &ci);

    std::cout << "  " << PINK << " >  " << RESET << WHITE;

    std::string line;
    while (true) {
        char c = static_cast<char>(_getch());
        if (c == '\r' || c == '\n') break;
        if (c == '\b' || c == 127) {
            if (!line.empty()) {
                line.pop_back();
                std::cout << "\b \b";
            }
            continue;
        }
        if (c < 32) continue;
        line += c;
        std::cout << '*';
    }
    std::cout << RESET << "\n";

    ci.bVisible = FALSE;
    ci.dwSize = 1;
    SetConsoleCursorInfo(hOut, &ci);

    return line;
}

void console::wait_exit() {
    std::cout << "\n";
    separator();
    std::cout << "  " << GRAY << "Press any key to exit..." << RESET << "\n";
    _getch();
}

void console::clear() {
    system("cls");
}
