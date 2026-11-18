#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace skullgirls {

struct ArchiveEntry {
    std::string name;
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t alignment;
};

std::vector<ArchiveEntry> read_archive_index(const std::filesystem::path& path);
std::vector<ArchiveEntry> read_archive_index(std::istream& input);

}
