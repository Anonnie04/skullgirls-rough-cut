#include "runtime_data.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("rough-cut-paths-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path directory;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(directory, error); }
    } cleanup{root};
    try {
        const auto data = root / "runtime/data02";
        std::filesystem::create_directories(data);
        const auto check = [](bool passed) { if (!passed) throw std::runtime_error("Runtime path resolution failed"); };
        for (const auto* name : {"core.gfs", "ui.gfs", "ui-win.gfs", "sprites.gfs", "characters-foits.gfs",
                                 "characters-art.gfs", "stages.gfs", "stages-textures.gfs", "stages-textures2d.gfs"})
            std::ofstream{data / name};
        check(skullgirls::resolve_runtime_data(root / "runtime/skullgirls-main-menu.exe") == data);
        check(skullgirls::resolve_runtime_data(root / "build/Release/skullgirls-main-menu.exe") == data);
        check(skullgirls::resolve_runtime_data(root / "unused.exe", data) == data);
        check(skullgirls::resolve_runtime_data(root / "unused.exe", root / "runtime") == data);
        bool rejected = false;
        try { skullgirls::resolve_runtime_data(root / "unused.exe", root / "missing"); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected);
        std::filesystem::remove(data / "core.gfs");
        check(!skullgirls::is_runtime_data(data));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
