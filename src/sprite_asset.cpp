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

CharacterAssets::CharacterAssets(const std::filesystem::path& directory, const std::string& character)
    : directory_(directory), character_(character), prefix_("characters/" + character),
      foit_(read_sprite_asset(directory_ / "characters-foits.gfs", prefix_ + ".foit")),
      main_art_(read_sprite_asset(directory_ / "characters-art.gfs", prefix_ + ".art"))
{
}

std::vector<SpriteFrame> CharacterAssets::read_frames() const
{
    return read_foit_frames(foit_, main_art_.size());
}

FoitAnimationAsset CharacterAssets::read_animations(bool read_children) const
{
    return read_foit_animation_asset(foit_, main_art_.size(), read_children);
}

FoitAnimationAsset CharacterAssets::read_animations_for_preview(bool require_children) const
{
    try {
        return read_animations(true);
    } catch (const std::runtime_error&) {
        if (require_children || character_ == "filia") throw;
        // The debug names follow the child tables. Keep root playback available
        // for files whose child metadata the reader cannot handle.
        return read_animations();
    }
}

SpriteBytes CharacterAssets::read_default_palette() const
{
    if (!is_versioned_foit(foit_)) return read_legacy_palette(foit_);
    return read_dds_palette(read_sprite_asset(directory_ / "sprites.gfs", "sprites/" + character_ + "_1p.dds"));
}

const SpriteBytes* CharacterAssets::find_art(std::uint8_t bank)
{
    if (bank == 5) return &main_art_;
    if (bank != 4) return nullptr;
    if (!alternate_art_) {
        const auto path = directory_ / "characters-art-pt.gfs";
        if (std::filesystem::exists(path)) {
            alternate_art_ = SpriteArchive(path).read_if_present(prefix_ + ".art-pt").value_or(SpriteBytes{});
        } else {
            alternate_art_.emplace();
        }
    }
    return alternate_art_->empty() ? nullptr : &*alternate_art_;
}

const SpriteBytes& CharacterAssets::art(std::uint8_t bank)
{
    if (const auto* bytes = find_art(bank)) return *bytes;
    if (bank == 4) throw std::runtime_error("Missing alternate sprite art: " + character_);
    throw std::runtime_error("Unsupported sprite art bank: " + std::to_string(bank));
}

}
