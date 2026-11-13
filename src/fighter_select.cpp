#include "fighter_select.hpp"

#include <cstring>
#include <stdexcept>
#include <string_view>

namespace skullgirls {

static FighterSelectLayout read_layout(const SpriteBytes& scene, bool second_player, PedestalLayout* pedestals)
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
    constexpr std::array base_ids{0x5abu, 0x5a7u, 0x5a9u, 0x5b1u, 0x5afu, 0x5b0u};
    constexpr std::array anchor_ids{0x613u, 0x614u, 0x612u, 0x615u, 0x616u, 0x617u};
    std::array<TitleRect, 6> local_anchors{};
    constexpr std::array name_ids{0x5acu, 0x5a8u, 0x5aau, 0x623u, 0x626u, 0x625u};
    constexpr std::array group_ids{0x62au, 0x62cu, 0x62du, 0x632u, 0x633u, 0x634u};
    std::array<TitleRect, 6> name_parents{};
    std::array<std::array<unsigned, 2>, 6> name_tracks{};
    std::array<bool, 6> anchor_found{}, base_found{}, key_found{};
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
            if (pedestals) {
                const auto id = number(at + 8);
                for (std::size_t j = 0; j < base_ids.size(); ++j) {
                    if (id == group_ids[j] && tag == "PRGG") name_parents[j] = {x, y, 0, 0};
                    if (id == base_ids[j] && tag == "RPSG") {
                        pedestals->bases[j / 3][j % 3] = rect;
                        base_found[j] = true;
                    }
                    if (id == anchor_ids[j] && tag == "LBLG") {
                        local_anchors[j] = {x, y, 0, 0};
                        anchor_found[j] = true;
                    }
                }
            }
            if (tag == "PRGG") {
                self(self, at + 0x74, at + length, number(at + 0x70), rect.x, rect.y, depth + 1);
            } else if (tag == "RPSG" || tag == "LBLG") {
                const auto label = name(at);
                if (label == "Portrait") { result.portrait = rect; found |= 1; }
                if (label == "PortraitName") { result.name = rect; found |= 2; }
                if (label == "Main Data Frame") { result.frame = rect; found |= 4; }
                if (label == "Name Slider") { result.bracket = rect; found |= 8; }
                if (label == "Player Number") { result.player_label = rect; found |= 16; }
                if (label == "Player Number Text") { result.player_number = rect; found |= 32; }
                if (label.size() == 8 && label.substr(0, 7) == "Circle " &&
                    label.back() >= '1' && label.back() <= '4') {
                    const auto slot = label.back() - '1';
                    result.coins[second_player ? 3 - slot : slot] = rect;
                    found |= 64u << slot;
                }
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
        if (name(view) == (pedestals ? "CharSelectFighters" :
                          second_player ? "CharSelectPickFighterP2" : "CharSelectPickFighterP1")) {
            const auto objects = number(view + 0x84);
            if (objects > size - 0x8c) throw std::runtime_error("Invalid fighter select object table");
            walk(walk, view + 0x8c + objects, view + size, number(view + 0x78), 0, 0, 0);
            if (pedestals) {
                const auto view_end = view + size;
                const auto offset = number(view + 0x88);
                if (offset > size - 0x8c || size - 0x8c - offset < 12)
                    throw std::runtime_error("Missing pedestal animation table");
                const auto table = view + 0x8c + offset;
                auto clip = table + 12;
                for (unsigned k = 0, n = number(table + 8); k < n; ++k) {
                    if (view_end - clip < 96) throw std::runtime_error("Truncated pedestal clip");
                    const auto length = number(clip + 4);
                    if (length < 96 || length > view_end - clip)
                        throw std::runtime_error("Invalid pedestal clip");
                    auto clip_name = bytes.substr(clip + 8, 64);
                    clip_name = clip_name.substr(0, clip_name.find('\0'));
                    if (clip_name.size() == 15 && clip_name[0] == 'P' &&
                        (clip_name[1] == '1' || clip_name[1] == '2') &&
                        clip_name.substr(2, 2) == "_F" && clip_name[4] >= '1' && clip_name[4] <= '3' &&
                        (clip_name.substr(5) == "_NameIntro" || clip_name.substr(5) == "_NameOutro")) {
                        const auto side = clip_name[1] - '1';
                        const auto slot = clip_name[4] - '1';
                        const auto index = side * 3 + slot;
                        const int outro = clip_name.substr(5) == "_NameOutro";
                        auto& keys = pedestals->nameplates[side][slot][outro].keys;
                        auto track = clip + 96;
                        const auto end = clip + length;
                        for (unsigned t = 0, count = number(clip + 84); t < count; ++t) {
                            if (end - track < 16) throw std::runtime_error("Truncated nameplate track");
                            const auto size = number(track + 4);
                            if (size < 16 || size > end - track) throw std::runtime_error("Invalid nameplate track");
                            const auto id = number(track + 8);
                            if (id == group_ids[index] || id == name_ids[index]) {
                                const bool group = id == group_ids[index];
                                auto key = track + 16;
                                if (number(track + 12) != keys.size()) throw std::runtime_error("Invalid nameplate key count");
                                for (auto& value : keys) {
                                    const auto minimum = group ? 84u : 92u;
                                    if (track + size - key < minimum) throw std::runtime_error("Truncated nameplate key");
                                    const auto stride = number(key + 4);
                                    if (stride < minimum || stride > track + size - key ||
                                        bytes.substr(key + 52, 4) != (group ? "TLPG" : "TLPS"))
                                        throw std::runtime_error("Invalid nameplate key");
                                    value.tick = number(key + 8);
                                    value.bounds.x += real(key + 68) + (group ? name_parents[index].x : 0);
                                    value.bounds.y += real(key + 72) + (group ? name_parents[index].y : 0);
                                    if (!group) {
                                        value.bounds.width = real(key + 76);
                                        value.bounds.height = real(key + 80);
                                        value.alpha = number(key + 88) >> 24;
                                    }
                                    key += stride;
                                }
                                name_tracks[index][outro] |= group ? 1 : 2;
                            }
                            track += size;
                        }
                    }
                    if (clip_name == "Intro") {
                        auto track = clip + 96;
                        const auto end = clip + length;
                        for (unsigned t = 0, count = number(clip + 84); t < count; ++t) {
                            if (end - track < 16) throw std::runtime_error("Truncated pedestal track");
                            const auto track_size = number(track + 4);
                            if (track_size < 16 || track_size > end - track)
                                throw std::runtime_error("Invalid pedestal track");
                            for (std::size_t j = 0; j < anchor_ids.size(); ++j) {
                                if (number(track + 8) != anchor_ids[j]) continue;
                                const auto key = track + 16;
                                if (track_size < 92 || !number(track + 12) || number(key + 8) != 0 ||
                                    bytes.substr(key + 52, 4) != "TLBL")
                                    throw std::runtime_error("Invalid pedestal placement key");
                                auto anchor = local_anchors[j];
                                anchor.x += real(key + 68);
                                anchor.y += real(key + 72);
                                pedestals->anchors[j / 3][j % 3] = anchor;
                                key_found[j] = true;
                            }
                            track += track_size;
                        }
                    }
                    clip += length;
                }
            }
            break;
        }
        view += size;
    }
    if (pedestals) {
        for (std::size_t i = 0; i < anchor_ids.size(); ++i)
            if (!base_found[i] || !anchor_found[i] || !key_found[i] || name_tracks[i][0] != 3 || name_tracks[i][1] != 3)
                throw std::runtime_error("Missing character-select pedestal placement");
        return result;
    }
    if (found != 1023) throw std::runtime_error("Missing fighter select layout");
    return result;
}

FighterSelectLayout read_fighter_select_layout(const SpriteBytes& scene, bool second_player)
{
    return read_layout(scene, second_player, nullptr);
}

PedestalLayout read_pedestal_layout(const SpriteBytes& scene)
{
    PedestalLayout layout;
    read_layout(scene, false, &layout);
    return layout;
}

}
