#pragma once

#include "sprite_rle.hpp"
#include <algorithm>
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

// CPU counterpart of the installed Otter_fp Palettized portrait shader.
// Resolve palette/shade/ink before filtering the finished image: interpolating
// the PCX color indices themselves would blend unrelated palette groups.
inline std::vector<std::uint32_t> colorize_hud_portrait(
    const HudPortraitLayer& colors, const HudPortraitLayer& shade,
    const HudPortraitLayer& lines, const SpriteBytes& palette)
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
        // The PCX groups are five indices apart, centered at 3, 8, 13, ...;
        // the fighter palette uses row zero as the transparent/background row.
        const unsigned row = index ? std::min<std::size_t>(index / 5 + 1, palette.size() / 64 - 1) : 0;
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
