#include "fighter_select.hpp"

#include <cstring>
#include <stdexcept>
#include <string_view>

namespace skullgirls {

FighterSelectLayout read_fighter_select_layout(const SpriteBytes& scene, bool second_player)
{
    const std::string_view bytes(reinterpret_cast<const char*>(scene.data()), scene.size());
    const auto number = [&](std::size_t at) {
        if (at > bytes.size() || bytes.size() - at < 4) throw std::runtime_error("Truncated Charselect.gbs");
        std::uint32_t value;
        std::memcpy(&value, bytes.data() + at, sizeof(value));
        return value;
    };
    const auto real = [&](std::size_t at) {
        const auto bits = number(at);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    };
    const auto name = [&](std::size_t at) {
        const auto field = bytes.substr(at + 12, 64);
        return field.substr(0, field.find('\0'));
    };
    if (bytes.substr(0, 4) != "CSGG" || bytes.size() < 56)
        throw std::runtime_error("Invalid Charselect.gbs header");

    FighterSelectLayout result{};
    unsigned found = 0;
    const auto walk = [&](auto&& self, std::size_t at, std::size_t end, unsigned count,
                          float x, float y, unsigned depth) -> void {
        if (depth > 32) throw std::runtime_error("Invalid character select nesting");
        for (unsigned i = 0; i < count; ++i) {
            if (at > end || end - at < 0x74) throw std::runtime_error("Truncated character select object");
            const auto length = number(at + 4);
            if (length < 0x74 || length > end - at) throw std::runtime_error("Invalid character select object");
            const TitleRect rect{x + real(at + 0x4c), y + real(at + 0x50),
                                 real(at + 0x5c), real(at + 0x60)};
            const auto tag = bytes.substr(at, 4);
            if (tag == "PRGG") {
                self(self, at + 0x74, at + length, number(at + 0x70), rect.x, rect.y, depth + 1);
            } else if (tag == "RPSG") {
                const auto label = name(at);
                if (label == "Portrait") { result.portrait = rect; found |= 1; }
                if (label == "PortraitName") { result.name = rect; found |= 2; }
                if (label == "Main Data Frame") { result.frame = rect; found |= 4; }
            }
            at += length;
        }
    };
    std::size_t view = 56ull + number(48);
    for (unsigned i = 0, count = number(28); i < count; ++i) {
        if (view > bytes.size() || bytes.size() - view < 0x8c ||
            bytes.substr(view, 4) != "WVGG") throw std::runtime_error("Invalid character select view");
        const auto size = number(view + 4);
        if (size < 0x8c || size > bytes.size() - view) throw std::runtime_error("Truncated character select view");
        if (name(view) == (second_player ? "CharSelectPickFighterP2" : "CharSelectPickFighterP1")) {
            const auto objects = number(view + 0x84);
            if (objects > size - 0x8c) throw std::runtime_error("Invalid fighter select object table");
            walk(walk, view + 0x8c + objects, view + size, number(view + 0x78), 0, 0, 0);
            break;
        }
        view += size;
    }
    if (found != 7) throw std::runtime_error("Missing fighter select layout");
    return result;
}

}
