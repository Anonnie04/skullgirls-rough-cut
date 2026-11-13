#pragma once
#include "sprite_rle.hpp"

namespace skullgirls {
struct SelectFrame {
    unsigned width = 0, height = 0, ticks = 0;
    float origin_x = 0, origin_y = 0;
    SpriteBytes pixels;
};
std::vector<SelectFrame> read_select_idle(const SpriteBytes& sprite, const SpriteBytes& dds);
}
