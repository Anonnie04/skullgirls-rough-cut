#include "sprite_asset.hpp"
#include "gfs.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace skullgirls {

SpriteBytes read_sprite_asset(const std::filesystem::path& archive, const std::string& name)
{
    const auto index = skullgirls::read_archive_index(archive);
    auto legacy_name = "temp/" + name.substr(name.find('/') + 1);
    if (legacy_name.compare(5, 10, "bossmarie.") == 0) {
        legacy_name.insert(9, "_");
    }
    const auto entry = std::find_if(index.begin(), index.end(), [&](const auto& item) {
        return item.name == name || item.name == legacy_name;
    });
    if (entry == index.end() || entry->size > skullgirls::SpriteBytes{}.max_size()) {
        throw std::runtime_error("Missing or oversized asset: " + name);
    }
    std::ifstream input(archive, std::ios::binary);
    skullgirls::SpriteBytes data(static_cast<std::size_t>(entry->size));
    if (!input.seekg(entry->offset) ||
        !input.read(reinterpret_cast<char*>(data.data()), data.size())) {
        throw std::runtime_error("Cannot read asset: " + name);
    }
    return data;
}

}
