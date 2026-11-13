#pragma once

#include "sprite_rle.hpp"
#include <array>
#include <map>
#include <string>
#include <utility>

namespace skullgirls {

struct TitleRect { float x, y, width, height; };

struct SceneOpacity {
    std::vector<std::pair<unsigned, unsigned>> keys;
    unsigned frames = 0;
    unsigned sample(double seconds, bool loop) const;
};

struct TitleScene {
    SceneOpacity prompt_opacity;
    SceneOpacity menu_opacity;
    float intro_horizontal = 0, intro_vertical = 0;

    struct Glyph { int x, y, width, height, top, advance, bearing; };
    struct Sprite { TitleRect bounds; std::string texture; };
    struct ShieldFrame { Sprite sprite; SceneOpacity opacity; };
    std::vector<ShieldFrame> shield;
    std::array<Sprite, 2> logo;
    TitleRect prompt_bounds{};
    std::string prompt;
    std::string font_atlas;
    int font_top = 0;
    unsigned atlas_width = 0, atlas_height = 0;
    std::map<unsigned, Glyph> glyphs;
};

TitleScene read_title_scene(const SpriteBytes& scene);

}
