#pragma once

#include "title_scene.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <random>
#include <stdexcept>
#include <string_view>

namespace skullgirls {

// Stage IDs are ASCII keys in the original flat localization dictionary. Read
// their JSON string values, retaining UTF-8 and decoding JSON escapes.
inline std::optional<std::string> stage_localized_name(std::string_view json, std::string_view id)
{
    const auto needle = "\"" + std::string(id) + "\"";
    auto at = json.find(needle);
    while (at != json.npos) {
        at += needle.size();
        at = json.find_first_not_of(" \t\r\n", at);
        if (at != json.npos && json[at] == ':') break;
        at = json.find(needle, at);
    }
    if (at == json.npos) return {};
    at = json.find_first_not_of(" \t\r\n", at + 1);
    if (at == json.npos || json[at++] != '"') throw std::runtime_error("Invalid stage localization string");
    const auto hex4 = [&]() {
        unsigned value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            if (at >= json.size()) throw std::runtime_error("Truncated stage localization escape");
            const auto c = json[at++];
            const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                              c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (digit < 0) throw std::runtime_error("Invalid stage localization escape");
            value = value * 16 + digit;
        }
        return value;
    };
    std::string value;
    while (at < json.size()) {
        char c = json[at++];
        if (c == '"') return value;
        if (static_cast<unsigned char>(c) < 32) throw std::runtime_error("Invalid stage localization character");
        if (c != '\\') { value += c; continue; }
        if (at >= json.size()) break;
        switch (c = json[at++]) {
        case '"': case '\\': case '/': value += c; break;
        case 'b': value += '\b'; break;
        case 'f': value += '\f'; break;
        case 'n': value += '\n'; break;
        case 'r': value += '\r'; break;
        case 't': value += '\t'; break;
        case 'u': {
            unsigned code = hex4();
            if (code >= 0xd800 && code <= 0xdbff) {
                if (at + 2 > json.size() || json.substr(at, 2) != "\\u") throw std::runtime_error("Missing localization surrogate");
                at += 2;
                const auto low = hex4();
                if (low < 0xdc00 || low > 0xdfff) throw std::runtime_error("Invalid localization surrogate");
                code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
            } else if (code >= 0xdc00 && code <= 0xdfff) throw std::runtime_error("Invalid localization surrogate");
            if (code < 0x80) value += char(code);
            else {
                if (code >= 0x10000) value += char(0xf0 | (code >> 18));
                if (code >= 0x800) value += char((code >= 0x10000 ? 0x80 : 0xe0) | ((code >> 12) & (code >= 0x10000 ? 63 : 15)));
                value += char((code >= 0x800 ? 0x80 : 0xc0) | ((code >> 6) & (code >= 0x800 ? 63 : 31)));
                value += char(0x80 | (code & 63));
            }
            break;
        }
        default: throw std::runtime_error("Invalid stage localization escape");
        }
    }
    throw std::runtime_error("Unterminated stage localization string");
}

struct StageSelectState {
    // Zero is the original's synthetic Random entry; real groups follow it.
    std::size_t choice = 0, variant = 0;
    std::optional<std::size_t> resolved;

    bool random() const { return choice == 0; }
    void reset() { *this = {}; }
    bool horizontal(int direction, const std::vector<std::vector<std::size_t>>& groups) {
        if (!direction || groups.empty()) return false;
        const auto count = groups.size() + 1;
        choice = direction < 0 ? (choice + count - 1) % count : (choice + 1) % count;
        variant = 0;
        resolved.reset();
        return true;
    }
    bool vertical(int direction, const std::vector<std::vector<std::size_t>>& groups) {
        if (!direction || random()) return false;
        const auto count = groups.at(choice - 1).size();
        if (count < 2) return false;
        variant = direction < 0 ? (variant + count - 1) % count : (variant + 1) % count;
        resolved.reset();
        return true;
    }
    std::optional<std::size_t> preview(const std::vector<std::vector<std::size_t>>& groups) const {
        if (random()) return {};
        return groups.at(choice - 1).at(variant);
    }
    template<class Generator>
    std::size_t confirm(const std::vector<std::vector<std::size_t>>& groups,
                        const std::vector<std::size_t>& eligible, Generator& generator) {
        if (resolved) return *resolved;
        if (const auto selected = preview(groups)) resolved = selected;
        else {
            if (eligible.empty()) throw std::runtime_error("No stages are eligible for Random");
            resolved = eligible[std::uniform_int_distribution<std::size_t>(0, eligible.size() - 1)(generator)];
        }
        return *resolved;
    }
    std::size_t selected(const std::vector<std::vector<std::size_t>>& groups) const {
        if (resolved) return *resolved;
        if (const auto selected = preview(groups)) return *selected;
        throw std::runtime_error("Random stage has not been confirmed");
    }
};

struct StageSelectLayout {
    TitleRect bar{}, name{}, header{}, header_text{}, preview{};
    std::array<TitleRect, 2> arrows{}, variant_arrows{};
    std::array<float, 2> variant_rotation{};
    // Authored OnActivate motion of the stage-name group (relative Y).
    std::vector<std::pair<unsigned, float>> entrance;
    unsigned entrance_frames = 0, fade_frames = 0;

    float entrance_offset(double seconds) const {
        if (entrance.empty()) return 0;
        const double tick = std::max(0.0, seconds * 60);
        const auto next = std::upper_bound(entrance.begin(), entrance.end(), tick,
            [](double t, const auto& key) { return t < key.first; });
        if (next == entrance.begin()) return next->second - entrance.back().second;
        const auto& previous = *(next - 1);
        if (next == entrance.end()) return 0;
        const auto fraction = (tick - previous.first) / (next->first - previous.first);
        return previous.second + (next->second - previous.second) * fraction - entrance.back().second;
    }
};

// Read the original stage scene, including pivots (the name-group pivot puts
// its controls near the bottom of the screen) and the two-preview fade timing.
inline StageSelectLayout read_stage_select_layout(const SpriteBytes& scene)
{
    const std::string_view bytes(reinterpret_cast<const char*>(scene.data()), scene.size());
    const auto u32 = [&](std::size_t at) {
        if (at > bytes.size() || bytes.size() - at < 4) throw std::runtime_error("Truncated stage-select scene");
        std::uint32_t value;
        std::memcpy(&value, bytes.data() + at, 4);
        return value;
    };
    const auto real = [&](std::size_t at) {
        const auto bits = u32(at);
        float value;
        std::memcpy(&value, &bits, 4);
        if (!std::isfinite(value)) throw std::runtime_error("Invalid stage-select coordinate");
        return value;
    };
    const auto name = [&](std::size_t at, std::size_t length) {
        if (at > bytes.size() || length > bytes.size() - at) throw std::runtime_error("Truncated stage-select name");
        const auto value = bytes.substr(at, length);
        return value.substr(0, value.find('\0'));
    };
    const auto end_of = [&](std::size_t at, std::size_t minimum, std::size_t limit) {
        if (at > limit || minimum > limit - at) throw std::runtime_error("Truncated stage-select record");
        const auto length = u32(at + 4);
        if (length < minimum || length > limit - at) throw std::runtime_error("Invalid stage-select record");
        return at + length;
    };
    if (bytes.size() < 56 || bytes.substr(0, 4) != "CSGG") throw std::runtime_error("Invalid stage-select scene");
    StageSelectLayout result;
    unsigned found = 0, name_group = 0;
    const auto walk = [&](auto&& self, std::size_t at, unsigned count, std::size_t limit,
                          float x, float y, unsigned depth) -> void {
        if (depth > 32 || at > limit || count > (limit - at) / 0x74) throw std::runtime_error("Invalid stage-select object tree");
        for (unsigned i = 0; i < count; ++i) {
            const auto end = end_of(at, 0x74, limit);
            const TitleRect rect{x + real(at + 0x4c) - real(at + 0x54), y + real(at + 0x50) - real(at + 0x58),
                                 real(at + 0x5c), real(at + 0x60)};
            const auto label = name(at + 12, 64);
            const auto tag = bytes.substr(at, 4);
            if (tag == "PRGG") {
                if (label == "Character Select") name_group = u32(at + 8);
                self(self, at + 0x74, u32(at + 0x70), end, rect.x, rect.y, depth + 1);
            } else {
                if (label == "Stage Name Frame") { result.bar = rect; found |= 1; }
                if (label == "StageNameLabel") { result.name = rect; found |= 2; }
                if (label == "Arrow Left Stage Select") { result.arrows[0] = rect; found |= 4; }
                if (label == "Arrow Right Stage Select") { result.arrows[1] = rect; found |= 8; }
                if (label == "Arrow Up Stage Select" || label == "Arrow Down Stage Select") {
                    const auto index = label == "Arrow Up Stage Select" ? 0 : 1;
                    result.variant_arrows[index] = rect;
                    result.variant_rotation[index] = real(at + 0x64);
                    found |= 16u << index;
                }
                if (label == "Header") { result.header = rect; found |= 64; }
                if (label == "Player Name") { result.header_text = rect; found |= 128; }
            }
            at = end;
        }
    };
    auto view = 56ull + u32(48);
    for (unsigned i = 0; i < u32(28); ++i) {
        const auto end = end_of(view, 0x8c, bytes.size());
        if (bytes.substr(view, 4) != "WVGG") throw std::runtime_error("Invalid stage-select view");
        const auto label = name(view + 12, 64);
        if (label == "CharSelectStage" || label == "CharSelectBackground_NEW") {
            if (label == "CharSelectStage") {
                const auto relative = u32(view + 0x84);
                if (relative > end - view - 0x8c) throw std::runtime_error("Invalid stage-select object offset");
                walk(walk, view + 0x8c + relative, u32(view + 0x78), end, 0, 0, 0);
            }
            const auto relative = u32(view + 0x88);
            if (relative > end - view - 0x8c) throw std::runtime_error("Invalid stage-select animation offset");
            const auto table = view + 0x8c + relative;
            const auto table_end = end_of(table, 12, end);
            auto clip = table + 12;
            for (unsigned j = 0; j < u32(table + 8); ++j) {
                const auto clip_end = end_of(clip, 96, table_end);
                const auto animation = name(clip + 8, 64);
                if ((label == "CharSelectStage" && animation == "OnActivate") ||
                    (label == "CharSelectBackground_NEW" && animation == "Stage_Fade_Up")) {
                    if (animation == "OnActivate") result.entrance_frames = u32(clip + 0x48);
                    else result.fade_frames = u32(clip + 0x48);
                    auto track = clip + 96;
                    for (unsigned k = 0; k < u32(clip + 0x54); ++k) {
                        const auto track_end = end_of(track, 16, clip_end);
                        auto key = track + 16;
                        for (unsigned n = 0; n < u32(track + 12); ++n) {
                            const auto key_end = end_of(key, 84, track_end);
                            if (animation == "OnActivate" && u32(track + 8) == name_group)
                                result.entrance.emplace_back(u32(key + 8), real(key + 72));
                            if (animation == "Stage_Fade_Up" && u32(track + 8) == 1 && n == 0)
                                result.preview = {real(key + 68), real(key + 72), real(key + 76), real(key + 80)};
                            key = key_end;
                        }
                        track = track_end;
                    }
                }
                clip = clip_end;
            }
        }
        view = end;
    }
    if (found != 255 || !result.fade_frames || result.preview.width <= 0 || result.entrance.empty())
        throw std::runtime_error("Missing stage-select layout or animations");
    return result;
}

}
