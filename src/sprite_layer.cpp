#include "sprite_layer.hpp"

#include <stdexcept>

namespace skullgirls {
namespace {

constexpr std::size_t tile_side = 16;

// Restore the delta-coded rows before interpolating the palette indices between them.
void restore_palette_indices(SpriteBytes& pixels, std::size_t stride)
{
    for (std::size_t y = 2; y < tile_side; y += 2) {
        for (std::size_t x = 0; x < stride; ++x) {
            auto& pixel = pixels[y * stride + x];
            pixel = static_cast<std::uint8_t>(pixel + pixels[(y - 2) * stride + x]);
        }
    }
    for (std::size_t x = 0; x < stride; ++x) {
        auto& last = pixels[15 * stride + x];
        last = static_cast<std::uint8_t>(last + pixels[14 * stride + x]);
    }
    for (std::size_t y = 1; y < 15; y += 2) {
        for (std::size_t x = 0; x < stride; ++x) {
            const auto above = pixels[(y - 1) * stride + x];
            const auto below = pixels[(y + 1) * stride + x];
            const auto high = ((above >> 4) + (below >> 4)) / 2;
            const auto low = ((above & 15) + (below & 15)) / 2;
            pixels[y * stride + x] = static_cast<std::uint8_t>((high << 4) | low);
        }
    }
}

void restore_brightness(SpriteBytes& pixels, std::size_t stride)
{
    for (std::size_t y = 1; y < 15; y += 2) {
        for (std::size_t x = 0; x < stride; ++x) {
            auto& pixel = pixels[y * stride + x];
            const auto above = pixels[(y - 1) * stride + x] & 31;
            const auto below = pixels[(y + 1) * stride + x] & 31;
            pixel = static_cast<std::uint8_t>((pixel & 224) | ((above + below) / 2));
        }
    }
}

}

SpriteBytes decode_sprite_layer(const SpriteLayer& layer, const SpriteBytes& art,
                                std::array<bool, 3> channels)
{
    const std::size_t columns = layer.tile_columns;
    const std::size_t rows = layer.tile_rows;
    const auto tile_count = columns * rows;
    if (layer.tile_mask.size() != (tile_count + 7) / 8) {
        throw std::runtime_error("Invalid sprite tile mask size");
    }

    // Only occupied tiles are stored in the art bank. Keep their original grid positions.
    std::vector<std::size_t> occupied;
    for (std::size_t tile = 0; tile < tile_count; ++tile) {
        if (layer.tile_mask[tile / 8] & (1u << (tile % 8))) {
            occupied.push_back(tile);
        }
    }
    if (occupied.size() != layer.used_tiles) {
        throw std::runtime_error("Sprite tile mask disagrees with used tile count");
    }

    const auto width = columns * tile_side;
    SpriteBytes image(width * rows * tile_side * channels.size());
    const auto stride = occupied.size() * tile_side;
    auto offset = layer.art_offset;
    for (std::size_t channel = 0; channel < channels.size(); ++channel) {
        const auto size = layer.channel_sizes[channel];
        if (offset > art.size() || size > art.size() - offset) {
            throw std::runtime_error("Sprite channel extends beyond the art bank");
        }
        const auto start = offset;
        offset += size;
        if (!channels[channel]) {
            continue;
        }

        auto pixels = decode_sprite_rle(
            SpriteBytes(art.begin() + start, art.begin() + offset), stride * tile_side);
        if (channel == 0) {
            restore_palette_indices(pixels, stride);
        } else if (channel == 1 && layer.interpolate_brightness) {
            restore_brightness(pixels, stride);
        }

        // The decoded channel packs occupied tiles side by side. Scatter them into the image grid.
        for (std::size_t tile = 0; tile < occupied.size(); ++tile) {
            const auto left = (occupied[tile] % columns) * tile_side;
            const auto top = (occupied[tile] / columns) * tile_side;
            for (std::size_t y = 0; y < tile_side; ++y) {
                for (std::size_t x = 0; x < tile_side; ++x) {
                    const auto source = y * stride + tile * tile_side + x;
                    const auto target = ((top + y) * width + left + x) * channels.size();
                    image[target + channel] = pixels[source];
                }
            }
        }
    }
    return image;
}

}
