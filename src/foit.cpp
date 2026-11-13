#include "foit.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace skullgirls {
namespace {

class Reader {
public:
    Reader(const SpriteBytes& bytes, std::size_t position = 0)
        : bytes_(bytes), position_(position) {}

    std::size_t take(std::size_t size)
    {
        if (position_ > bytes_.size() || size > bytes_.size() - position_) {
            throw std::runtime_error("Truncated sprite metadata");
        }
        const auto start = position_;
        position_ += size;
        return start;
    }

    std::uint32_t number(unsigned size)
    {
        const auto start = take(size);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < size; ++i) {
            value |= std::uint32_t(bytes_[start + i]) << (8 * i);
        }
        return value;
    }

    std::int32_t signed_word()
    {
        const auto bits = number(4);
        std::int32_t value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

private:
    const SpriteBytes& bytes_;
    std::size_t position_;
};

SpriteLayer read_layer(Reader& reader, const SpriteBytes& foit, bool legacy)
{
    const auto tiles = reader.number(2);
    const auto columns = reader.number(2);
    const auto rows = reader.number(2);
    const auto used = reader.number(2);
    const auto flags = reader.number(1);
    if (columns > 255 || rows > 255 || tiles != columns * rows || used > tiles
        || (flags & 0x28) != 0x28) {
        throw std::runtime_error("Invalid FOIT layer");
    }

    if (flags & 4) {
        reader.take(8);
    }
    const auto mask_size = (tiles + 7) / 8;
    SpriteBytes mask(mask_size);
    if (flags & 16) {
        const auto start = reader.take(mask_size);
        std::copy_n(foit.begin() + start, mask_size, mask.begin());
    }
    unsigned occupied = 0;
    for (auto byte : mask) {
        while (byte) {
            byte &= byte - 1;
            ++occupied;
        }
    }
    if (occupied != used || (tiles % 8 && mask.back() >> (tiles % 8))) {
        throw std::runtime_error("Invalid FOIT tile mask");
    }

    SpriteLayer layer{static_cast<std::uint8_t>(columns), static_cast<std::uint8_t>(rows),
                      static_cast<std::uint16_t>(used), bool(flags & 64), std::move(mask),
                      0, {}};
    if (legacy && tiles == 0) {
        return layer;
    }
    layer.art_offset = reader.number(4);
    for (auto& size : layer.channel_sizes) {
        size = reader.number(4);
    }
    return layer;
}

}

WorldBox foit_world_box(const FoitBox& box, std::int32_t x, std::int32_t y, bool facing_right)
{
    // FUN_0049e350, before the optional character scaling in FUN_004a1280.
    const std::int64_t world_x = x, world_y = y;
    return {world_y - box[0], world_y - box[1],
            facing_right ? world_x + box[2] : world_x - box[3],
            facing_right ? world_x + box[3] : world_x - box[2]};
}

bool foit_boxes_overlap(const WorldBox& a, const WorldBox& b)
{
    // FUN_0049e470 at 0049e9ed uses strict rejection, so edge contact counts.
    return a[2] <= b[3] && b[2] <= a[3] && a[0] >= b[1] && b[0] >= a[1];
}

std::vector<SpriteFrame> read_foit_frames(const SpriteBytes& foit, std::size_t main_art_size)
{
    const bool legacy = !is_versioned_foit(foit);
    if (!legacy) {
        Reader header(foit, 8);
        const auto version = header.number(4);
        if (version != 3 && version != 9) {
            throw std::runtime_error("Unsupported FOIT version");
        }
    }

    // Locate the frame table using the first layer, as in Sprite Extractor.
    for (std::size_t start = 5; start + 41 < foit.size() && start + 11 < 300000; ++start) {
        if (foit[start - 5] != 1 || foit[start + 9] != 0 || !(foit[start + 10] & 1)) {
            continue;
        }
        Reader reader(foit, start - 4);
        const auto count = reader.number(4);
        if (count == 0 || count >= 20000 || count > (foit.size() - start) / 57) {
            continue;
        }
        try {
            Reader first(foit, start + 11);
            const auto layer = read_layer(first, foit, legacy);
            const bool legacy_empty = legacy && !layer.tile_columns && !layer.tile_rows;
            if ((!legacy_empty && !layer.used_tiles) || layer.tile_columns >= 150 || layer.tile_rows >= 150
                || layer.art_offset > main_art_size) {
                continue;
            }
            auto remaining = main_art_size - layer.art_offset;
            bool fits = true;
            for (auto size : layer.channel_sizes) {
                if (!size || size > remaining) {
                    fits = false;
                    break;
                }
                remaining -= size;
            }
            if (!fits && !legacy_empty) {
                continue;
            }

            std::vector<SpriteFrame> frames;
            for (std::uint32_t i = 0; i < count; ++i) {
                SpriteFrame frame{};
                frame.origin = {reader.signed_word(), reader.signed_word()};
                frame.art_bank = static_cast<std::uint8_t>(reader.number(1));
                reader.take(reader.number(1) * 12);
                const auto layers = reader.number(1);
                for (unsigned slot = 0; slot < frame.layers.size(); ++slot) {
                    if (layers & (1u << slot)) {
                        frame.layers[slot] = read_layer(reader, foit, legacy);
                    }
                }
                frame.primary_layer_offset = {reader.signed_word(), reader.signed_word()};
                reader.take(32);
                for (auto& group : frame.box_groups) {
                    const auto boxes = reader.number(2);
                    for (std::uint32_t j = 0; j < boxes; ++j) {
                        group.push_back({reader.signed_word(), reader.signed_word(),
                                         reader.signed_word(), reader.signed_word()});
                    }
                }
                frames.push_back(std::move(frame));
            }
            return frames;
        } catch (const std::runtime_error&) {
            // A matching byte pattern can appear before the actual frame table.
        }
    }
    throw std::runtime_error("No valid FOIT frame table");
}

SpriteBytes read_dds_palette(const SpriteBytes& dds)
{
    Reader header(dds);
    if (header.number(4) != 0x20534444 || header.number(4) != 124) {
        throw std::runtime_error("Invalid palette DDS header");
    }
    header.take(4);
    const auto rows = header.number(4);
    const auto width = header.number(4);
    header.take(56);
    if (width != 16 || !rows || header.number(4) != 32 || header.number(4) != 0x41
        || header.number(4) != 0 || header.number(4) != 32
        || header.number(4) != 0xff0000 || header.number(4) != 0xff00
        || header.number(4) != 0xff || header.number(4) != 0xff000000
        || dds.size() < 128 || rows > (dds.size() - 128) / 64
       ) {
        throw std::runtime_error("Unsupported palette DDS or sprite channels");
    }
    return SpriteBytes(dds.begin() + 128, dds.begin() + 128 + std::size_t(rows) * 64);
}

bool is_versioned_foit(const SpriteBytes& foit)
{
    Reader header(foit, 4);
    return header.number(4) == 0x4e535256;
}

SpriteBytes read_legacy_palette(const SpriteBytes& foit)
{
    SpriteBytes palette;
    for (unsigned trailer : {8, 10}) {
        SpriteBytes candidate;
        try {
            Reader header(foit);
            Reader reader(foit, header.number(4));
            reader.take(reader.number(1));
            reader.take(4);
            if (reader.number(1) != 1) {
                continue;
            }
            const auto count = reader.number(2);
            if (count == 0 || count > 256) {
                continue;
            }
            std::size_t colors = 0;
            unsigned color_count = 0;
            for (unsigned i = 0; i < count; ++i) {
                const auto size = reader.number(2);
                if (size == 0 || size > 256) {
                    throw std::runtime_error("Invalid legacy color count");
                }
                const auto offset = reader.take(size * 4);
                if (i == 0) {
                    colors = offset;
                    color_count = size;
                }
                reader.take(trailer);
            }
            const auto groups = reader.number(1);
            unsigned next = 0;
            candidate.resize(color_count * 64);
            for (unsigned group = 0; group < groups; ++group) {
                const auto low = reader.number(1);
                const auto high = reader.number(1);
                reader.take(2);
                const auto length = reader.number(1);
                const auto label = reader.take(length);
                if (low != next || high < low || high >= color_count || !length ||
                    !std::all_of(foit.begin() + label, foit.begin() + label + length,
                                 [](auto c) { return c >= 32 && c < 127; })) {
                    throw std::runtime_error("Invalid legacy palette group");
                }
                for (unsigned shade = 0; shade < 16; ++shade) {
                    const double x = shade == 15 ? double(high - low) : shade * ((high - low) / 15.0);
                    const auto left = static_cast<unsigned>(x);
                    const auto right = std::min(left + 1, high - low);
                    const double t = x - left;
                    for (unsigned channel = 0; channel < 4; ++channel) {
                        const auto a = foit[colors + (high - left) * 4 + channel];
                        const auto b = foit[colors + (high - right) * 4 + channel];
                        const auto value = static_cast<std::uint8_t>(std::floor(a * (1 - t) + b * t));
                        for (unsigned row = low; row <= high; ++row) {
                            candidate[row * 64 + shade * 4 + channel] = value;
                        }
                    }
                }
                next = high + 1;
            }
            if (next != color_count) {
                continue;
            }
        } catch (const std::runtime_error&) {
            continue;
        }
        if (!palette.empty()) {
            throw std::runtime_error("Ambiguous legacy palette layout");
        }
        palette.swap(candidate);
    }
    if (palette.empty()) {
        throw std::runtime_error("Cannot read legacy palette");
    }
    return palette;
}

SpriteBytes colorize_sprite_with_palette(const SpriteBytes& pixels, const SpriteBytes& palette)
{
    const auto rows = palette.size() / 64;
    if (!rows || palette.size() % 64 || pixels.size() % 3) {
        throw std::runtime_error("Invalid palette or sprite channels");
    }
    SpriteBytes rgba;
    rgba.reserve(pixels.size() / 3 * 4);
    for (std::size_t i = 0; i < pixels.size(); i += 3) {
        const auto green = pixels[i + 1];
        const auto row = std::min<std::size_t>(pixels[i + 2], rows - 1);
        const double x = (green % 32) / 31.0 * 15.0;
        const auto left = static_cast<unsigned>(x);
        const auto right = std::min(left + 1, 15u);
        const double fraction = x - left;
        const double black = (green / 32) / 7.0;
        for (int component : {2, 1, 0, 3}) {
            const auto base = row * 64 + component;
            const double color = palette[base + left * 4] * (1 - fraction)
                               + palette[base + right * 4] * fraction;
            const double shaded = component == 3
                ? std::max(std::fmod(color, 128.0) * 2, black * 255)
                : color * (1 - black);
            rgba.push_back(static_cast<std::uint8_t>(std::nearbyint(std::clamp(shaded, 0.0, 255.0))));
        }
    }
    return rgba;
}

SpriteBytes colorize_sprite(const SpriteBytes& pixels, const SpriteBytes& dds)
{
    return colorize_sprite_with_palette(pixels, read_dds_palette(dds));
}

}
