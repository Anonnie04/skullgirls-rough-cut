// Decode one compressed sprite channel into a raw byte file.

#include "sprite_rle.hpp"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::cerr << "Usage: skullgirls-rle <decoded-size> <compressed-channel> <output-file>\n";
        return 2;
    }

    try {
        std::size_t size = 0;
        const std::string_view argument(argv[1]);
        const auto end = argument.data() + argument.size();
        const auto result = std::from_chars(argument.data(), end, size);
        if (result.ec != std::errc{} || result.ptr != end) {
            throw std::runtime_error("Invalid decoded size");
        }

        std::ifstream input(std::filesystem::path(argv[2]), std::ios::binary);
        if (!input) {
            throw std::runtime_error("Cannot open compressed channel");
        }
        const skullgirls::SpriteBytes compressed(
            std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
        const auto decoded = skullgirls::decode_sprite_rle(compressed, size);
        std::ofstream output(std::filesystem::path(argv[3]), std::ios::binary);
        output.write(reinterpret_cast<const char*>(decoded.data()), decoded.size());
        output.close();
        if (!output) {
            throw std::runtime_error("Cannot write decoded channel");
        }
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
