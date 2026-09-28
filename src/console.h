#pragma once
#include <string>
#include <vector>

namespace console {

    void init();
    void logo();
    void separator();
    void blank();

    void info(const std::string& msg);
    void success(const std::string& msg);
    void error(const std::string& msg);
    void warning(const std::string& msg);
    void step(const std::string& msg);

    void progress(const std::string& label, int current, int total);

    int select(const std::string& prompt, const std::vector<std::string>& options);

    std::string input(const std::string& prompt);
    std::string input_masked(const std::string& prompt);

    void wait_exit();
    void clear();

}
