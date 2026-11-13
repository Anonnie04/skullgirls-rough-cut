#pragma once

#include "title_scene.hpp"

namespace skullgirls {

struct FighterSelectLayout {
    TitleRect portrait, name, frame;
};

FighterSelectLayout read_fighter_select_layout(const SpriteBytes& scene, bool second_player);

}
