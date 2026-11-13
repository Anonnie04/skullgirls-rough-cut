#include "gfs.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace skullgirls {
namespace {

class IndexReader {
public:
    explicit IndexReader(const std::filesystem::path& path)
        : input_(path, std::ios::binary | std::ios::ate)
    {
        const auto end = input_.tellg();
        if (end < 0 || !input_.seekg(0)) {
            throw std::runtime_error("Cannot open GFS archive");
        }
        limit_ = size_ = static_cast<std::uint64_t>(end);
    }

    std::uint64_t size() const { return size_; }
    std::uint64_t remaining() const { return limit_ - position_; }

    void limit_to_index(std::uint64_t data_offset)
    {
        if (data_offset < position_ || data_offset > size_) {
            throw std::runtime_error("Invalid GFS data offset");
        }
        limit_ = data_offset;
    }

    template <typename UInt>
    UInt big_endian()
    {
        unsigned char bytes[sizeof(UInt)];
        read(reinterpret_cast<char*>(bytes), sizeof(bytes));
        UInt value = 0;
        for (auto byte : bytes) {
            value = (value << 8) | byte;
        }
        return value;
    }

    std::string string()
    {
        const auto length = big_endian<std::uint64_t>();
        if (length > remaining() || length > 32768) {
            throw std::runtime_error("Invalid GFS string length");
        }
        std::string value(static_cast<std::size_t>(length), '\0');
        read(value.data(), value.size());
        return value;
    }

private:
    void read(char* target, std::size_t length)
    {
        if (length > remaining() || !input_.read(target, static_cast<std::streamsize>(length))) {
            throw std::runtime_error("Truncated GFS index");
        }
        position_ += length;
    }

    std::ifstream input_;
    std::uint64_t size_ = 0;
    std::uint64_t limit_ = 0;
    std::uint64_t position_ = 0;
};

}

std::vector<ArchiveEntry> read_archive_index(const std::filesystem::path& path)
{
    IndexReader reader(path);
    const auto data_offset = reader.big_endian<std::uint32_t>();
    reader.limit_to_index(data_offset);
    // The game's FUN_004e8370 checks both fields before reading the table.
    if (reader.string() != "Reverge Package File" || reader.string() != "1.1") {
        throw std::runtime_error("Unsupported GFS signature or version");
    }

    const auto count = reader.big_endian<std::uint64_t>();
    constexpr auto minimum_entry_size = sizeof(std::uint64_t) * 2 + sizeof(std::uint32_t);
    if (count > reader.remaining() / minimum_entry_size) {
        throw std::runtime_error("GFS entry count exceeds the index");
    }

    std::vector<ArchiveEntry> entries;
    std::uint64_t position = data_offset;
    for (std::uint64_t i = 0; i < count; ++i) {
        ArchiveEntry entry{};
        entry.name = reader.string();
        entry.size = reader.big_endian<std::uint64_t>();
        entry.alignment = reader.big_endian<std::uint32_t>();
        if (entry.name.empty() || entry.alignment == 0) {
            throw std::runtime_error("Invalid GFS entry");
        }

        const auto alignment = entry.alignment;
        const auto padding = (alignment - position % alignment) % alignment;
        if (padding > reader.size() - position || entry.size > reader.size() - position - padding) {
            throw std::runtime_error("Truncated GFS payload");
        }
        entry.offset = position + padding;
        position = entry.offset + entry.size;
        entries.push_back(std::move(entry));
    }
    return entries;
}

}
