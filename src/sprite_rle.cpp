#include "sprite_rle.hpp"

#include <algorithm>
#include <stdexcept>

namespace skullgirls {

SpriteBytes decode_sprite_rle(const SpriteBytes& compressed, std::size_t decoded_size)
{
    SpriteBytes decoded;
    std::size_t position = 0;
    const auto read_byte = [&]() {
        if (position == compressed.size()) {
            throw std::runtime_error("Truncated sprite RLE");
        }
        return compressed[position++];
    };

    while (decoded.size() < decoded_size) {
        // The high bit selects a repeated byte. The low bits encode the run length.
        const auto control = read_byte();
        const unsigned code = control & 0x7f;
        std::size_t count = code + 1;
        // Control codes 125 through 127 read a one- to three-byte extended length.
        if (code >= 125) {
            count = 0;
            for (unsigned i = 0; i < code - 124; ++i) {
                count = (count << 8) | read_byte();
            }
            count = std::max<std::size_t>(1, count);
        }
        if (count > decoded_size - decoded.size()) {
            throw std::runtime_error("Sprite RLE run exceeds decoded size");
        }

        if (control & 0x80) {
            const auto value = read_byte();
            decoded.insert(decoded.end(), count, value);
        } else {
            if (count > compressed.size() - position) {
                throw std::runtime_error("Truncated sprite RLE literal");
            }
            const auto begin = compressed.begin() + position;
            decoded.insert(decoded.end(), begin, begin + count);
            position += count;
        }
    }
    if (position != compressed.size()) {
        throw std::runtime_error("Trailing sprite RLE bytes");
    }
    return decoded;
}

}
