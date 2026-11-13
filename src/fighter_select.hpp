#pragma once

#include "title_scene.hpp"

namespace skullgirls {

struct FighterSelectLayout {
    TitleRect portrait, name, frame, bracket, player_label, player_number;
    std::array<TitleRect, 4> coins{};
};

struct NameplateClip {
    struct Key { unsigned tick; TitleRect bounds; unsigned alpha; };
    std::array<Key, 2> keys{};
};

struct PedestalLayout {
    std::array<std::array<std::array<NameplateClip, 2>, 3>, 2> nameplates{};
    std::array<std::array<TitleRect, 3>, 2> bases{};
    std::array<std::array<TitleRect, 3>, 2> anchors{};
};
PedestalLayout read_pedestal_layout(const SpriteBytes& scene);

FighterSelectLayout read_fighter_select_layout(const SpriteBytes& scene, bool second_player);

}
