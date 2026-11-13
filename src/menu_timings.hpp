#pragma once

#include "sprite_rle.hpp"

namespace skullgirls {

struct MenuTimings {
    float up, down, heading, open, close;
};

MenuTimings read_menu_timings(const SpriteBytes& scene);

}
