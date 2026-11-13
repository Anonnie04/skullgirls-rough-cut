#include "foit.hpp"
#include "sprite_asset.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void composite_over(skullgirls::SpriteBytes& base, const skullgirls::SpriteBytes& top)
{
    // Match Pillow's integer source-over rounding with seven fractional bits.
    const auto divide_by_255 = [](unsigned value) {
        return ((value >> 8) + value) >> 8;
    };
    for (std::size_t i = 0; i < base.size(); i += 4) {
        const unsigned source_alpha = top[i + 3];
        if (source_alpha == 0) {
            continue;
        }
        const unsigned destination_weight = base[i + 3] * (255 - source_alpha);
        const unsigned alpha = source_alpha * 255 + destination_weight;
        const unsigned source_weight = source_alpha * 255 * 255 * 128 / alpha;
        const unsigned base_weight = 255 * 128 - source_weight;
        for (unsigned channel = 0; channel < 3; ++channel) {
            const auto value = top[i + channel] * source_weight + base[i + channel] * base_weight;
            base[i + channel] = static_cast<std::uint8_t>(divide_by_255(value + 128 * 128) >> 7);
        }
        base[i + 3] = static_cast<std::uint8_t>(divide_by_255(alpha + 128));
    }
}

std::size_t number(std::string_view text)
{
    std::size_t value = 0;
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::runtime_error("Invalid frame or layer number");
    }
    return value;
}

std::int32_t coordinate(std::string_view text)
{
    std::int32_t value = 0;
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end)
        throw std::runtime_error("Invalid world coordinate");
    return value;
}

}

int main(int argc, char** argv)
{
    const bool boxes = (argc == 5 || argc == 8) && std::string_view(argv[4]) == "boxes";
    if (argc != 6 && !boxes) {
        std::cerr << "Usage: skullgirls-sprite <data-directory> <character> <frame> <layer|frame> <output.pam>\n"
                  << "       skullgirls-sprite <data-directory> <character> <frame> boxes [x y left|right]\n";
        return 2;
    }
    try {
        const std::filesystem::path directory(argv[1]);
        const std::string character(argv[2]);
        const std::string prefix = "characters/" + character;
        const auto frame_index = number(argv[3]);
        const bool whole_frame = std::string_view(argv[4]) == "frame";
        const auto foit = skullgirls::read_sprite_asset(directory / "characters-foits.gfs", prefix + ".foit");
        auto art = skullgirls::read_sprite_asset(directory / "characters-art.gfs", prefix + ".art");
        const auto frames = skullgirls::read_foit_frames(foit, art.size());
        const auto& frame = frames.at(frame_index);
        if (boxes) {
            const bool world = argc == 8;
            const auto x = world ? coordinate(argv[5]) : 0;
            const auto y = world ? coordinate(argv[6]) : 0;
            const std::string_view facing = world ? argv[7] : "right";
            if (facing != "right" && facing != "left") throw std::runtime_error("Invalid facing direction");
            std::cout << "origin\t" << frame.origin[0] << '\t' << frame.origin[1] << '\n';
            for (std::size_t group = 0; group < frame.box_groups.size(); ++group) {
                for (const auto& box : frame.box_groups[group]) {
                    std::cout << group;
                    if (world) {
                        for (auto edge : skullgirls::foit_world_box(box, x, y, facing == "right"))
                            std::cout << '\t' << edge;
                    } else {
                        for (auto edge : box) std::cout << '\t' << edge;
                    }
                    std::cout << '\n';
                }
            }
            return std::cout ? 0 : 1;
        }
        const auto slot = whole_frame ? 0 : number(argv[4]);
        if (frame.art_bank == 4) {
            art = skullgirls::read_sprite_asset(directory / "characters-art-pt.gfs", prefix + ".art-pt");
        } else if (frame.art_bank != 5) {
            throw std::runtime_error("Unsupported sprite art bank");
        }
        const auto palette = skullgirls::is_versioned_foit(foit)
            ? skullgirls::read_dds_palette(skullgirls::read_sprite_asset(directory / "sprites.gfs", "sprites/" + character + "_1p.dds"))
            : skullgirls::read_legacy_palette(foit);
        const skullgirls::SpriteLayer* dimensions = nullptr;
        skullgirls::SpriteBytes rgba;
        const auto layer_count = whole_frame ? 2u : 1u;
        for (std::size_t i = slot; i - slot < layer_count; ++i) {
            const auto& layer = frame.layers.at(i);
            if (!layer || !layer->tile_columns || !layer->tile_rows) {
                continue;
            }
            if (dimensions && (dimensions->tile_columns != layer->tile_columns ||
                               dimensions->tile_rows != layer->tile_rows)) {
                throw std::runtime_error("Frame layer sizes differ");
            }
            auto colored = skullgirls::colorize_sprite_with_palette(skullgirls::decode_sprite_layer(*layer, art), palette);
            if (dimensions) {
                composite_over(rgba, colored);
            } else {
                rgba.swap(colored);
                dimensions = &*layer;
            }
        }
        if (!dimensions) {
            throw std::runtime_error(whole_frame ? "Frame is empty" : "Frame layer is empty");
        }

        std::ofstream output(argv[5], std::ios::binary);
        output << "P7\nWIDTH " << dimensions->tile_columns * 16 << "\nHEIGHT " << dimensions->tile_rows * 16
               << "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
        output.write(reinterpret_cast<const char*>(rgba.data()), rgba.size());
        output.close();
        if (!output) {
            throw std::runtime_error("Cannot write sprite image");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
