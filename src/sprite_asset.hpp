#pragma once

#include "sprite_rle.hpp"
#include <filesystem>
#include <string>

namespace skullgirls {
SpriteBytes read_sprite_asset(const std::filesystem::path& archive, const std::string& name);
}
