#pragma once

#include "sprite_rle.hpp"

#include <array>

namespace skullgirls {

struct SpriteLayer {
    std::uint8_t tile_columns;
    std::uint8_t tile_rows;
    std::uint16_t used_tiles;
    bool interpolate_brightness;
    SpriteBytes tile_mask;
    std::size_t art_offset;
    std::array<std::size_t, 3> channel_sizes;
};

// Returns three interleaved channel bytes per pixel in row-major order.
SpriteBytes decode_sprite_layer(const SpriteLayer& layer, const SpriteBytes& art,
                                std::array<bool, 3> channels = {true, true, true});

}
