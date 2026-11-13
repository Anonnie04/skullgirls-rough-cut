#include "menu_timings.hpp"

#include <array>
#include <stdexcept>
#include <string_view>

namespace skullgirls {

MenuTimings read_menu_timings(const SpriteBytes& scene)
{
    const std::string_view bytes(reinterpret_cast<const char*>(scene.data()), scene.size());
    const auto u32 = [&](std::size_t at) {
        if (at > bytes.size() || bytes.size() - at < 4)
            throw std::runtime_error("Truncated Main.gbs");
        return std::uint32_t(scene[at]) | (std::uint32_t(scene[at + 1]) << 8)
            | (std::uint32_t(scene[at + 2]) << 16) | (std::uint32_t(scene[at + 3]) << 24);
    };
    if (bytes.substr(0, 4) != "CSGG" || bytes.size() < 56)
        throw std::runtime_error("Invalid Main.gbs header");

    std::size_t view = 56ull + u32(48);
    for (std::uint32_t i = 0, count = u32(28); i < count; ++i) {
        const auto size = u32(view + 4);
        if (view > bytes.size() || size < 0x8c || size > bytes.size() - view ||
            bytes.substr(view, 4) != "WVGG")
            throw std::runtime_error("Invalid Main.gbs view");
        if (bytes.substr(view + 12, 13) == std::string_view("MainMenu_New\0", 13)) {
            const auto end = view + size;
            const auto relative = u32(view + 0x88);
            if (relative > size - 0x8c || size - 0x8c - relative < 12)
                throw std::runtime_error("Missing MainMenu_New animations");
            const auto table = view + 0x8c + relative;
            const auto table_size = u32(table + 4);
            if (bytes.substr(table, 4) != "LAGG" || table_size < 12 || table_size > end - table)
                throw std::runtime_error("Invalid MainMenu_New animation table");

            constexpr std::array<std::string_view, 5> names{
                "MenuUp", "MenuDown", "MenuCenterSwap", "SubMenuIntro", "SubMenuOutro"
            };
            std::array<float, names.size()> seconds{};
            std::size_t animation = table + 12;
            const auto table_end = table + table_size;
            for (std::uint32_t j = 0, n = u32(table + 8); j < n; ++j) {
                if (table_end - animation < 96 || bytes.substr(animation, 4) != "NAGG")
                    throw std::runtime_error("Invalid MainMenu_New animation record");
                const auto length = u32(animation + 4);
                if (length < 96 || length > table_end - animation)
                    throw std::runtime_error("Truncated MainMenu_New animation record");
                auto name = bytes.substr(animation + 8, 64);
                name = name.substr(0, name.find('\0'));
                for (std::size_t k = 0; k < names.size(); ++k) {
                    if (name != names[k]) continue;
                    const auto frames = u32(animation + 0x48);
                    if (frames < 2 || frames > 600 || seconds[k] != 0)
                        throw std::runtime_error("Invalid MainMenu_New frame count");
                    // The executable samples this clip from frame 0 through frame count minus one.
                    seconds[k] = (frames - 1) / 60.0f;
                }
                animation += length;
            }
            for (const auto duration : seconds) {
                if (duration == 0) throw std::runtime_error("Missing MainMenu_New clip");
            }
            return {seconds[0], seconds[1], seconds[2], seconds[3], seconds[4]};
        }
        view += size;
    }
    throw std::runtime_error("Missing MainMenu_New view");
}

}
