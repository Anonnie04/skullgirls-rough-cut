#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace skullgirls {

using SpriteBytes = std::vector<std::uint8_t>;

SpriteBytes decode_sprite_rle(const SpriteBytes& compressed, std::size_t decoded_size);

}
