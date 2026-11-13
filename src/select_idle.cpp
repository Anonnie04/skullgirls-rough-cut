#include "select_idle.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace skullgirls {
std::vector<SelectFrame> read_select_idle(const SpriteBytes& sprite, const SpriteBytes& dds)
{
    std::size_t position = 0;
    const auto read = [&](std::size_t length) {
        if (length > sprite.size() - position) throw std::runtime_error("Truncated select sprite");
        const auto start = position;
        position += length;
        return sprite.data() + start;
    };
    const auto number = [&](unsigned length) {
        std::uint64_t value = 0;
        const auto bytes = read(length);
        for (unsigned i = 0; i < length; ++i) value = (value << 8) | bytes[i];
        return value;
    };
    const auto real = [&]() {
        const auto bits = static_cast<std::uint32_t>(number(4));
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) throw std::runtime_error("Invalid select sprite origin");
        return value;
    };
    const auto string = [&]() {
        const auto length = number(8);
        if (length > sprite.size() - position) throw std::runtime_error("Invalid select sprite string");
        return std::string_view(reinterpret_cast<const char*>(read(static_cast<std::size_t>(length))),
                                static_cast<std::size_t>(length));
    };
    if (string() != "2.0") throw std::runtime_error("Unsupported select sprite version");
    string();
    read(4);
    if (string() != "unigned char tile_x, tile_y, tile_u, tile_v;" || number(8) != 4)
        throw std::runtime_error("Unsupported select tile format");
    const auto tile_count = number(8), frame_count = number(8);
    if (!tile_count || tile_count > 65536 || !frame_count || frame_count > 512 ||
        number(8) != 1 || number(8) != 16 || number(8) != 16)
        throw std::runtime_error("Unsupported select sprite layout");

    const auto word = [&](unsigned at) {
        return unsigned(dds[at]) | (unsigned(dds[at + 1]) << 8) |
               (unsigned(dds[at + 2]) << 16) | (unsigned(dds[at + 3]) << 24);
    };
    if (dds.size() < 128 || word(0) != 0x20534444 || word(4) != 124 || word(80) != 64 ||
        word(84) != 0 || word(88) != 16 || word(92) != 0xf800 || word(96) != 0x7e0 ||
        word(100) != 0x1f || word(104) != 0)
        throw std::runtime_error("Invalid select RGB565 atlas");
    const auto height = word(12), width = word(16);
    if (!width || !height || width > 4096 || height > 4096 || width % 16 || height % 16 ||
        dds.size() != 128 + width * height * 2)
        throw std::runtime_error("Invalid select atlas dimensions");

    const auto tiles = read(static_cast<std::size_t>(tile_count) * 4);
    std::vector<SelectFrame> frames;
    for (std::uint64_t i = 0; i < frame_count; ++i) {
        const auto first = number(4), count = number(4), ticks = number(4);
        SelectFrame frame;
        frame.origin_x = real();
        frame.origin_y = real();
        if (first > tile_count || count > tile_count - first || ticks == 0)
            throw std::runtime_error("Invalid select frame");
        frame.ticks = static_cast<unsigned>(ticks);
        for (auto j = first; j < first + count; ++j) {
            frame.width = std::max(frame.width, (tiles[j * 4] + 1u) * 16);
            frame.height = std::max(frame.height, (tiles[j * 4 + 1] + 1u) * 16);
        }
        frame.pixels.resize(frame.width * frame.height * 3);
        for (auto j = first; j < first + count; ++j) {
            const auto* tile = tiles + j * 4;
            if ((tile[2] + 1u) * 16 > width || (tile[3] + 1u) * 16 > height)
                throw std::runtime_error("Select tile outside atlas");
            for (unsigned y = 0; y < 16; ++y) {
                for (unsigned x = 0; x < 16; ++x) {
                    const auto source = 128 + 2 * ((tile[3] * 16 + y) * width + tile[2] * 16 + x);
                    const unsigned packed = dds[source] | (unsigned(dds[source + 1]) << 8);
                    const auto dest = 3 * ((tile[1] * 16 + y) * frame.width + tile[0] * 16 + x);
                    frame.pixels[dest + 2] = (packed >> 5) & 63;
                    frame.pixels[dest + 1] = (packed & 31) | (((31 - (packed >> 11)) * 7 + 15) / 31 << 5);
                    frame.pixels[dest] = packed == 0xf81f ? 0 : 1;
                }
            }
        }
        frames.push_back(std::move(frame));
    }
    if (string() != "idle" || number(8) != frame_count || number(8) != frame_count - 1)
        throw std::runtime_error("Unsupported select idle sequence");
    return frames;
}
}
