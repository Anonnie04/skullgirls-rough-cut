#pragma once

#include "gfs.hpp"
#include "sprite_rle.hpp"
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
}
