#pragma once

#include "sprite_layer.hpp"

#include <optional>

namespace skullgirls {

using FoitBox = std::array<std::int32_t, 4>;
using WorldBox = std::array<std::int64_t, 4>;

// Unscaled world edges in engine order: upper Y, lower Y, left X, right X.
WorldBox foit_world_box(const FoitBox& box, std::int32_t x, std::int32_t y, bool facing_right);
bool foit_boxes_overlap(const WorldBox& a, const WorldBox& b);

struct SpriteFrame {
    std::uint8_t art_bank;
    std::array<std::optional<SpriteLayer>, 8> layers;
    std::array<std::int32_t, 2> origin;
    std::array<std::int32_t, 2> primary_layer_offset;
    std::array<std::vector<FoitBox>, 3> box_groups;
};

std::vector<SpriteFrame> read_foit_frames(const SpriteBytes& foit, std::size_t main_art_size);
bool is_versioned_foit(const SpriteBytes& foit);
SpriteBytes read_dds_palette(const SpriteBytes& dds);
SpriteBytes read_legacy_palette(const SpriteBytes& foit);
SpriteBytes colorize_sprite_with_palette(const SpriteBytes& pixels, const SpriteBytes& palette);
SpriteBytes colorize_sprite(const SpriteBytes& pixels, const SpriteBytes& palette_dds);

}
