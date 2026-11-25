#pragma once

#include "sprite_rle.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace skullgirls {

struct HudPortraitLayer {
    unsigned width, height;
    SpriteBytes pixels;
};

inline HudPortraitLayer read_hud_portrait_layer(const SpriteBytes& bytes)
{
    if (bytes.size() < 128 || bytes[0] != 10 || bytes[2] != 1 || bytes[3] != 8 || bytes[65] != 1)
        throw std::runtime_error("Unsupported HUD portrait PCX");
    const auto word = [&](unsigned offset) { return unsigned(bytes[offset]) | (unsigned(bytes[offset + 1]) << 8); };
    if (word(8) < word(4) || word(10) < word(6))
        throw std::runtime_error("Invalid HUD portrait bounds");
    const unsigned width = word(8) - word(4) + 1, height = word(10) - word(6) + 1;
    const unsigned pitch = word(66);
    if (pitch < width || pitch > 2048 || height > 2048)
        throw std::runtime_error("Invalid HUD portrait size");
    HudPortraitLayer layer{width, height, {}};
    layer.pixels.reserve(width * height);
    std::size_t offset = 128;
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < pitch;) {
            if (offset >= bytes.size()) throw std::runtime_error("Truncated HUD portrait PCX");
            auto pixel = bytes[offset++];
            unsigned count = 1;
            if ((pixel & 0xc0) == 0xc0) {
                count = pixel & 0x3f;
                if (!count || count > pitch - x || offset >= bytes.size())
                    throw std::runtime_error("Invalid HUD portrait pixel run");
                pixel = bytes[offset++];
            }
            if (x < width) layer.pixels.insert(layer.pixels.end(), std::min(count, width - x), pixel);
            x += count;
        }
    }
    return layer;
}

using HudPaletteMap = std::array<std::uint8_t, 256>;

inline HudPaletteMap read_hud_palette_map(const SpriteBytes& foit)
{
    HudPaletteMap result{};
    std::size_t position = 0;
    const auto read = [&](unsigned count) {
        if (position > foit.size() || count > foit.size() - position)
            throw std::runtime_error("Truncated HUD palette metadata");
        std::uint32_t value = 0;
        for (unsigned i=0;i<count;++i) value |= std::uint32_t(foit[position++]) << (i*8);
        return value;
    };
    const auto metadata = read(4);
    if (read(4) != 0x4e535256) {
        // Legacy sprite palettes already expand each source index to a shade row.
        for (unsigned i=0;i<result.size();++i) result[i]=std::uint8_t(i);
        return result;
    }
    const auto version=read(4);
    if (version!=3 && version!=9) throw std::runtime_error("Unsupported HUD palette metadata");
    position=metadata;
    const auto skip_string=[&] { const auto length=read(1); for(unsigned i=0;i<length;++i) read(1); };
    skip_string(); read(4);
    if (read(1)!=1) throw std::runtime_error("Missing HUD palette groups");
    read(2); // Number of selectable palettes.
    const auto groups=read(1);
    if (!groups) throw std::runtime_error("Empty HUD palette groups");
    unsigned index=0;
    for(unsigned group=0;group<groups;++group) {
        const auto count=read(1);
        read(2); // Lighting and group flags.
        skip_string();
        if (!count || count>result.size()-index) throw std::runtime_error("Invalid HUD palette group size");
        for(unsigned i=0;i<count;++i) result[index++]=std::uint8_t(group);
    }
    return result;
}

// CPU counterpart of the installed Otter_fp Palettized portrait shader.
// Resolve palette/shade/ink before filtering the finished image: interpolating
// the PCX color indices themselves would blend unrelated palette groups.
inline std::vector<std::uint32_t> colorize_hud_portrait(
    const HudPortraitLayer& colors, const HudPortraitLayer& shade,
    const HudPortraitLayer& lines, const SpriteBytes& palette, const HudPaletteMap& palette_map)
{
    const auto size = std::size_t(colors.width) * colors.height;
    if (!size || colors.pixels.size() != size || shade.pixels.size() != size || lines.pixels.size() != size ||
        colors.width != shade.width || colors.width != lines.width ||
        colors.height != shade.height || colors.height != lines.height)
        throw std::runtime_error("Mismatched HUD portrait layers");
    if (palette.empty() || palette.size() % 64)
        throw std::runtime_error("Invalid HUD portrait palette");
    std::vector<std::uint32_t> result(size);
    for (std::size_t i = 0; i < size; ++i) {
        const unsigned index = colors.pixels[i];
        const double ink = std::floor((255 - lines.pixels[i]) * 16.0 / 255) / 16;
        if (!index && !ink) continue;
        // Group sizes vary by character, including reserved transparent bands.
        const unsigned row = std::min<std::size_t>(palette_map[index], palette.size() / 64 - 1);
        const double intensity = std::clamp((shade.pixels[i] - 100.0) / 155.0, 0.0, 1.0);
        const double position = intensity * 15;
        const unsigned left = static_cast<unsigned>(position), right = std::min(left + 1, 15u);
        const double fraction = position - left;
        const auto sample = [&](unsigned channel) {
            return palette[row * 64 + left * 4 + channel] * (1 - fraction) +
                   palette[row * 64 + right * 4 + channel] * fraction;
        };
        // Alpha's upper bit is a lighting flag, not opacity. Decode it before
        // taking the maximum with the independent outline coverage.
        const double encoded_alpha = index ? sample(3) : 0;
        const double alpha = std::max((encoded_alpha - (encoded_alpha >= 128 ? 128 : 0)) * 2 / 255, ink);
        const auto byte = [](double v) { return std::uint32_t(std::lround(std::clamp(v, 0.0, 255.0))); };
        result[i] = byte(alpha * 255) << 24;
        if (index) {
            for (unsigned channel = 0; channel < 3; ++channel)
                result[i] |= byte(sample(channel) * (1 - ink)) << (channel * 8);
        }
    }
    return result;
}

}
