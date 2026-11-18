#pragma once

#include "sprite_layer.hpp"

#include <optional>
#include <string>

namespace skullgirls {

using FoitBox = std::array<std::int32_t, 4>;
using WorldBox = std::array<std::int64_t, 4>;

// Unscaled world edges in engine order: upper Y, lower Y, left X, right X.
WorldBox foit_world_box(const FoitBox& box, std::int32_t x, std::int32_t y, bool facing_right);
bool foit_boxes_overlap(const WorldBox& a, const WorldBox& b);

struct SpriteFrame {
    std::uint8_t art_bank;
    std::array<std::optional<SpriteLayer>, 8> layers;
    std::array<std::int32_t, 2> origin;
    std::array<std::int32_t, 2> primary_layer_offset;
    std::array<std::vector<FoitBox>, 3> box_groups;
};

std::vector<SpriteFrame> read_foit_frames(const SpriteBytes& foit, std::size_t main_art_size);

struct FoitOperand {
    std::uint8_t flags;
    std::uint32_t value;
};

struct FoitExpression {
    std::uint8_t opcode;
    bool comparison;
    FoitOperand left, right, destination;
};

struct FoitInputStep {
    std::array<std::uint8_t, 3> directions;
    std::array<std::uint8_t, 15> buttons;
    std::uint16_t hold;
    bool invert_direction;
    std::vector<FoitExpression> tests;
};

struct FoitCondition {
    std::uint16_t window, buffer;
    std::uint8_t flags;
    std::vector<FoitInputStep> steps;
    std::optional<FoitInputStep> reject;
};

struct FoitTransition {
    std::uint16_t target;
    FoitCondition condition;
};

struct FoitFrameVariant {
    FoitCondition condition;
    std::optional<std::uint16_t> duration;
    std::vector<FoitExpression> script;
};

struct FoitAnimationFrame {
    std::array<std::uint16_t, 3> sprites;
    std::uint16_t ticks;
    std::vector<FoitExpression> script;
    std::vector<FoitFrameVariant> variants;
};

using FoitAnimation = std::vector<FoitAnimationFrame>;

struct FoitMoveTable {
    std::vector<FoitAnimation> animations;
    std::vector<std::uint16_t> successors;
    std::vector<std::vector<FoitTransition>> transitions;
    std::vector<std::string> names;
    std::vector<FoitMoveTable> children;
};

struct FoitAnimationAsset : FoitMoveTable {
    std::vector<SpriteFrame> sprites;
};

// Reads the visual timelines in version 9 FOIT files.
FoitAnimationAsset read_foit_animation_asset(const SpriteBytes& foit, std::size_t main_art_size,
                                             bool read_children = false);

bool is_versioned_foit(const SpriteBytes& foit);
SpriteBytes read_dds_palette(const SpriteBytes& dds);
SpriteBytes read_legacy_palette(const SpriteBytes& foit);

class SpritePaletteLookup {
public:
    explicit SpritePaletteLookup(const SpriteBytes& palette);
    std::uint32_t argb(std::uint8_t index, std::uint8_t shade) const
    {
        return colors_[unsigned(index) * 256 + shade];
    }

private:
    std::vector<std::uint32_t> colors_;
};

SpriteBytes colorize_sprite_with_palette(const SpriteBytes& pixels, const SpriteBytes& palette);
SpriteBytes colorize_sprite(const SpriteBytes& pixels, const SpriteBytes& palette_dds);
void composite_sprite_rgba(SpriteBytes& base, const SpriteBytes& overlay);

}
