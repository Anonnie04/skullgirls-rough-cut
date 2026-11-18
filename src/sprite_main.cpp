#include "foit.hpp"
#include "sprite_asset.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

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
    const bool animations = (argc == 4 || argc == 5) && std::string_view(argv[3]) == "animations";
    if (argc != 6 && !boxes && !animations) {
        std::cerr << "Usage: skullgirls-sprite <data-directory> <character> <frame> <layer|frame> <output.pam>\n"
                  << "       skullgirls-sprite <data-directory> <character> <frame> boxes [x y left|right]\n"
                  << "       skullgirls-sprite <data-directory> <character> animations [[child:]move]\n";
        return 2;
    }
    try {
        const std::filesystem::path directory(argv[1]);
        skullgirls::CharacterAssets assets(directory, argv[2]);
        if (animations) {
            std::string_view selection = argc == 5 ? argv[4] : "";
            const auto colon = selection.find(':');
            const bool child = colon != selection.npos;
            const auto asset = assets.read_animations_for_preview(child);
            const auto& table = child ? asset.children.at(number(selection.substr(0, colon))) : asset;
            if (child) selection.remove_prefix(colon + 1);
            const auto from = selection.empty() ? 0 : number(selection);
            if (!selection.empty() && from >= table.animations.size())
                throw std::runtime_error("Animation index is out of range");
            const auto to = selection.empty() ? table.animations.size() : from + 1;
            std::cout << "move\tsuccessor\trecord\tticks\tsprite_1\tsprite_2\tsprite_3\tname\n";
            for (auto move = from; move < to; ++move) {
                const auto& clip = table.animations[move];
                const auto name = table.names.empty() ? std::string_view{} : std::string_view(table.names[move]);
                if (clip.empty()) std::cout << move << '\t' << table.successors[move] << "\t\t\t\t\t\t" << name << '\n';
                for (std::size_t i = 0; i < clip.size(); ++i) {
                    const auto& frame = clip[i];
                    std::cout << move << '\t' << table.successors[move] << '\t' << i << '\t' << frame.ticks;
                    for (auto sprite : frame.sprites) std::cout << '\t' << sprite;
                    std::cout << '\t' << name << '\n';
                }
            }
            return std::cout ? 0 : 1;
        }
        const auto frame_index = number(argv[3]);
        const bool whole_frame = std::string_view(argv[4]) == "frame";
        const auto frames = assets.read_frames();
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
        const auto& art = assets.art(frame.art_bank);
        const auto palette = assets.read_default_palette();
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
                skullgirls::composite_sprite_rgba(rgba, colored);
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
