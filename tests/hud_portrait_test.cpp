#include "hud_portrait.hpp"
#include <iostream>

int main() {
    try {
        const auto check = [](bool ok, const char* text) {
            if (!ok) throw std::runtime_error(text);
        };
        // A continuous gradient, with alpha's lighting flag set. An opaque
        // pixel must decode to 254 rather than treating 255 as ordinary alpha.
        skullgirls::SpriteBytes palette(3 * 64);
        for (unsigned row = 1; row < 3; ++row)
            for (unsigned x = 0; x < 16; ++x) {
                palette[row * 64 + x * 4] = x * 16;
                palette[row * 64 + x * 4 + 1] = row == 1 ? 100 : 200;
                palette[row * 64 + x * 4 + 2] = 150;
                palette[row * 64 + x * 4 + 3] = 255;
            }
        skullgirls::HudPortraitLayer colors{6, 1, {3, 3, 3, 0, 0, 8}};
        skullgirls::HudPortraitLayer shade{6, 1, {100, 255, 177, 255, 255, 255}};
        skullgirls::HudPortraitLayer lines{6, 1, {255, 255, 127, 255, 0, 255}};
        const auto pixels = skullgirls::colorize_hud_portrait(colors, shade, lines, palette);
        check((pixels[0] & 255) == 0 && (pixels[1] & 255) == 240, "Portrait shade endpoints");
        check(((pixels[0] >> 24) & 255) == 254, "Decode palette lighting flag");
        check(((pixels[2] >> 16) & 255) == 75, "Independent, sixteen-step outline");
        check(pixels[3] == 0, "Empty PCX background stays transparent");
        check(pixels[4] == 0xff000000, "Outline-only pixels remain opaque black");
        check(((pixels[5] >> 8) & 255) == 200, "PCX color-group remapping");
        shade.width = 5;
        bool rejected = false;
        try { skullgirls::colorize_hud_portrait(colors, shade, lines, palette); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Reject mismatched layer dimensions");
        std::cout << "HUD portrait shade, outline, transparency, palette and validation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
