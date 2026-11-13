#pragma once

#include "sprite_layer.hpp"

#include <optional>

namespace skullgirls {

struct SpriteFrame {
    std::uint8_t art_bank;
    std::array<std::optional<SpriteLayer>, 8> layers;
};

std::vector<SpriteFrame> read_foit_frames(const SpriteBytes& foit, std::size_t main_art_size);
bool is_versioned_foit(const SpriteBytes& foit);
SpriteBytes read_dds_palette(const SpriteBytes& dds);
SpriteBytes read_legacy_palette(const SpriteBytes& foit);
SpriteBytes colorize_sprite_with_palette(const SpriteBytes& pixels, const SpriteBytes& palette);
SpriteBytes colorize_sprite(const SpriteBytes& pixels, const SpriteBytes& palette_dds);

}
