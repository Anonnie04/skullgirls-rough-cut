#pragma once

#include "gfs.hpp"
#include "foit.hpp"
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

namespace skullgirls {
class SpriteArchive {
public:
    explicit SpriteArchive(const std::filesystem::path& path);

    const std::vector<ArchiveEntry>& entries() const { return entries_; }
    SpriteBytes read(const std::string& name);
    std::optional<SpriteBytes> read_if_present(const std::string& name);

private:
    std::ifstream input_;
    std::vector<ArchiveEntry> entries_;
    std::unordered_map<std::string, std::size_t> by_name_;
};

SpriteBytes read_sprite_asset(const std::filesystem::path& archive, const std::string& name);

class CharacterAssets {
public:
    CharacterAssets(const std::filesystem::path& directory, const std::string& character);

    std::vector<SpriteFrame> read_frames() const;
    FoitAnimationAsset read_animations(bool read_children = false) const;
    FoitAnimationAsset read_animations_for_preview(bool require_children) const;
    SpriteBytes read_default_palette() const;

    // The alternate art bank is loaded once, when a caller first asks for it.
    const SpriteBytes* find_art(std::uint8_t bank);
    const SpriteBytes& art(std::uint8_t bank);

private:
    std::filesystem::path directory_;
    std::string character_;
    std::string prefix_;
    SpriteBytes foit_;
    SpriteBytes main_art_;
    std::optional<SpriteBytes> alternate_art_;
};
}
