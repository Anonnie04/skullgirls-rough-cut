#include "sprite_asset.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace skullgirls {

SpriteArchive::SpriteArchive(const std::filesystem::path& path)
    : input_(path, std::ios::binary), entries_(read_archive_index(input_))
{
    by_name_.reserve(entries_.size());
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        by_name_.try_emplace(entries_[i].name, i);
    }
}

SpriteBytes SpriteArchive::read(const std::string& name)
{
    auto data = read_if_present(name);
    if (!data) throw std::runtime_error("Missing asset: " + name);
    return std::move(*data);
}

std::optional<SpriteBytes> SpriteArchive::read_if_present(const std::string& name)
{
    auto legacy_name = "temp/" + name.substr(name.find('/') + 1);
    if (legacy_name.compare(5, 10, "bossmarie.") == 0) {
        legacy_name.insert(9, "_");
    }
    auto found = by_name_.find(name);
    const auto legacy = by_name_.find(legacy_name);
    // Preserve first-match precedence for archives containing both name forms.
    if (legacy != by_name_.end() && (found == by_name_.end() || legacy->second < found->second)) {
        found = legacy;
    }
    if (found == by_name_.end()) {
        return std::nullopt;
    }
    const auto& entry = entries_[found->second];
    if (entry.size > SpriteBytes{}.max_size() ||
        entry.size > std::numeric_limits<std::streamsize>::max()) {
        throw std::runtime_error("Oversized asset: " + name);
    }
    SpriteBytes data(static_cast<std::size_t>(entry.size));
    input_.clear();
    if (!input_.seekg(static_cast<std::streamoff>(entry.offset)) ||
        (!data.empty() && !input_.read(reinterpret_cast<char*>(data.data()),
                                       static_cast<std::streamsize>(data.size())))) {
        throw std::runtime_error("Cannot read asset: " + name);
    }
    return data;
}

SpriteBytes read_sprite_asset(const std::filesystem::path& archive, const std::string& name)
{
    return SpriteArchive(archive).read(name);
}

}
