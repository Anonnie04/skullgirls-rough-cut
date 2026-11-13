#include "title_scene.hpp"

#include <cstring>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace skullgirls {

unsigned SceneOpacity::sample(double seconds, bool loop) const
{
    const double frame = seconds * 60;
    const double t = loop ? std::fmod(frame, frames) : frame;
    const auto next = std::upper_bound(keys.begin(), keys.end(), t,
        [](double position, const auto& key) { return position < key.first; });
    if (next == keys.begin()) return next->second;
    const auto& previous = *(next - 1);
    if (next == keys.end()) return previous.second;
    const auto fraction = (t - previous.first) / (next->first - previous.first);
    return static_cast<unsigned>(std::lround(previous.second * (1 - fraction) + next->second * fraction));
}

TitleScene read_title_scene(const SpriteBytes& scene)
{
    const std::string_view bytes(reinterpret_cast<const char*>(scene.data()), scene.size());
    const auto u32 = [&](std::size_t at) {
        if (at > bytes.size() || bytes.size() - at < 4)
            throw std::runtime_error("Truncated title scene");
        std::uint32_t value;
        std::memcpy(&value, bytes.data() + at, sizeof(value));
        return value;
    };
    const auto f32 = [&](std::size_t at) {
        const auto bits = u32(at);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    };
    const auto name_at = [&](std::size_t at, std::size_t size) {
        const auto name = bytes.substr(at, size);
        return name.substr(0, name.find('\0'));
    };
    const auto record_end = [&](std::size_t at, std::size_t minimum, std::size_t limit) {
        if (at > limit || limit - at < minimum)
            throw std::runtime_error("Truncated title scene record");
        const auto size = u32(at + 4);
        if (size < minimum || size > limit - at)
            throw std::runtime_error("Invalid title scene record length");
        return at + size;
    };

    if (bytes.substr(0, 4) != "CSGG" || bytes.size() < 56)
        throw std::runtime_error("Invalid Main.gbs header");

    TitleScene result;
    unsigned prompt_font = 0;
    std::size_t title_view = 0, title_end = 0;
    const auto read_opacity = [&](std::size_t view, std::size_t end, std::string_view clip,
                                  unsigned object, std::string_view transform, float* delta = nullptr, unsigned axis = 0) {
        const auto offset = u32(view + 0x88);
        if (offset > end - view - 0x8c) throw std::runtime_error("Invalid animation table offset");
        const auto table = view + 0x8c + offset;
        const auto table_end = record_end(table, 12, end);
        for (auto animation = table + 12; animation < table_end;) {
            const auto animation_end = record_end(animation, 96, table_end);
            if (bytes.substr(animation, 4) == "NAGG" && name_at(animation + 8, 64) == clip) {
                SceneOpacity result;
                result.frames = u32(animation + 0x48);
                auto track = animation + 96;
                for (unsigned i = 0, count = u32(animation + 0x54); i < count; ++i) {
                    const auto track_end = record_end(track, 16, animation_end);
                    if (bytes.substr(track, 4) == "CAGG" && u32(track + 8) == object) {
                        auto key = track + 16;
                        for (unsigned j = 0, n = u32(track + 12); j < n; ++j) {
                            const auto key_end = record_end(key, delta ? 76 : 92, track_end);
                            if (bytes.substr(key + 52, 4) != transform)
                                throw std::runtime_error("Unexpected scene opacity transform");
                            const auto frame = u32(key + 8);
                            if (!result.keys.empty() && frame <= result.keys.back().first)
                                throw std::runtime_error("Invalid scene opacity key");
                            if (delta) {
                                const auto position = f32(key + 68 + axis * 4);
                                if (j == 0) *delta = position;
                                else if (j + 1 == n) *delta -= position;
                            }
                            result.keys.emplace_back(frame, delta ? 0 : u32(key + 88) >> 24);
                            key = key_end;
                        }
                        if (!result.keys.empty()) return result;
                    }
                    track = track_end;
                }
            }
            animation = animation_end;
        }
        throw std::runtime_error("Missing title transition track");
    };
    const auto find_objects = [&](auto&& recurse, std::size_t at, std::size_t limit,
                                  unsigned count, float x, float y, unsigned depth) -> void {
        if (depth > 32) throw std::runtime_error("Invalid title scene nesting");
        for (unsigned i = 0; i < count; ++i) {
            const auto end = record_end(at, 0x74, limit);
            const auto tag = bytes.substr(at, 4);
            const auto name = name_at(at + 12, 64);
            const TitleRect rect{x + f32(at + 0x4c), y + f32(at + 0x50),
                                 f32(at + 0x5c), f32(at + 0x60)};
            if (tag == "PRGG") {
                recurse(recurse, at + 0x74, end, u32(at + 0x70), rect.x, rect.y, depth + 1);
            } else if (tag == "RPSG" && (name == "Shield_Frm_Top" || name == "Encore_Text" ||
                       name.substr(0, 14) == "Top_Shield_Frm" || name.substr(0, 17) == "Bottom_Shield_Frm")) {

                constexpr std::size_t texture_record_size = 0x11c;
                std::size_t texture = 56ull + u32(40);
                const auto id = u32(at + 0x70);
                for (unsigned j = 0, n = u32(20); j < n; ++j, texture += texture_record_size) {
                    if (texture > bytes.size() || bytes.size() - texture < texture_record_size)
                        throw std::runtime_error("Truncated title texture table");
                    if (u32(texture) != id) continue;
                    const auto path = name_at(texture + 4, texture_record_size - 4);
                    if (path.size() < 4 || path.substr(path.size() - 4) != ".png")
                        throw std::runtime_error("Invalid title texture path");
                    TitleScene::Sprite sprite{rect, "ui/Win/" + std::string(path.substr(0, path.size() - 4)) + ".dds"};
                    if (name == "Shield_Frm_Top" || name == "Encore_Text")
                        result.logo[name == "Shield_Frm_Top" ? 0 : 1] = std::move(sprite);
                    else
                        result.shield.push_back({std::move(sprite),
                            read_opacity(title_view, title_end, "OnActivate", u32(at + 8), "TLPS")});
                    break;
                }
            } else if (tag == "LBLG" && name == "PressButton") {
                if (end - at < 0xa0 || u32(at + 0x9c) > end - at - 0xa0)
                    throw std::runtime_error("Truncated title prompt");
                result.prompt_bounds = rect;
                result.prompt = name_at(at + 0xa0, u32(at + 0x9c));
                prompt_font = u32(at + 0x70);
            }
            at = end;
        }
    };

    std::size_t view = 56ull + u32(48);
    for (unsigned i = 0, count = u32(28); i < count; ++i) {
        const auto end = record_end(view, 0x8c, bytes.size());
        if (bytes.substr(view, 4) != "WVGG") throw std::runtime_error("Invalid Main.gbs view");
        const auto name = name_at(view + 12, 64);
        if (name == "MainMenu_New") {
            result.menu_opacity = read_opacity(view, end, "MenuIntro", 0xda, "TLPS");
            read_opacity(view, end, "MenuIntro", 0xbc, "TLPG", &result.intro_horizontal, 0);
            read_opacity(view, end, "MenuIntro", 0xe0, "TLPG", &result.intro_vertical, 1);
        }
        if (name == "Splash_2ND") {
            title_view = view;
            title_end = end;
            result.prompt_opacity = read_opacity(view, end, "PressButtonOn", 0x15e, "TLBL");
            const auto relative = u32(view + 0x84);
            if (relative > end - view - 0x8c) throw std::runtime_error("Invalid title object table");
            find_objects(find_objects, view + 0x8c + relative, end, u32(view + 0x78), 0, 0, 0);
        }
        view = end;
    }

    std::size_t font = 56ull + u32(36);
    for (unsigned i = 0, count = u32(16); i < count; ++i) {
        const auto end = record_end(font, 100, bytes.size());
        if (u32(font + 8) == prompt_font) {
            result.font_atlas = "ui/Win/Fonts/" + std::string(name_at(font + 12, 64)) + "_0.dds";
            result.atlas_width = u32(font + 80);
            result.atlas_height = u32(font + 84);
            if (!result.atlas_width || !result.atlas_height) throw std::runtime_error("Invalid title atlas size");
            result.font_top = static_cast<int>(u32(font + 88));
            const auto glyph_count = u32(font + 96);
            if (glyph_count > (end - font - 100) / 40) throw std::runtime_error("Truncated title font");
            for (unsigned j = 0; j < glyph_count; ++j) {
                const auto p = font + 100 + j * 40;
                if (u32(p + 36) != 0 || u32(p + 4) != 0) continue;
                result.glyphs.emplace(u32(p), TitleScene::Glyph{
                    int(u32(p + 8)), int(u32(p + 12)), int(u32(p + 16)), int(u32(p + 20)),
                    int(u32(p + 24)), int(u32(p + 28)), int(u32(p + 32))});
            }
            break;
        }
        font = end;
    }

    if (result.logo[0].texture.empty() || result.logo[1].texture.empty() ||
        result.shield.empty() || result.prompt.empty() || result.glyphs.empty() || result.menu_opacity.keys.empty())
        throw std::runtime_error("Missing Splash_2ND title assets");
    return result;
}

}
