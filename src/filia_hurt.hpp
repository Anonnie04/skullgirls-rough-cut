#pragma once

#include "filia_normals.hpp"

namespace skullgirls {

// Entry, hold, and two recovery branches from Filia's installed FOIT.
enum HurtGroup { stand_mid, crouch_high, crouch_low, stand_low, air_mid };

constexpr NormalFrame filia_hurt[5][4][4] = {
    {
        {{702, 1}, {702, 2}}, // Move 213
        {{703, 1}, {703, 2}}, // Move 214
        {{704, 1}, {704, 2}, {705, 2}, {706, 2}}, // Move 215
        {{707, 1}, {707, 2}, {708, 2}, {709, 300}}, // Move 216
    },
    {
        {{717, 1}, {717, 2}}, // Move 221
        {{718, 1}, {718, 2}}, // Move 222
        {{719, 1}, {719, 2}, {720, 2}, {721, 2}}, // Move 223
        {{722, 1}, {722, 3}, {723, 300}}, // Move 224
    },
    {
        {{724, 1}, {724, 2}}, // Move 225
        {{725, 1}, {725, 2}}, // Move 226
        {{726, 1}, {726, 3}, {727, 3}}, // Move 227
        {{728, 1}, {728, 300}}, // Move 228
    },
    {
        {{710, 1}, {710, 2}}, // Move 217
        {{711, 1}, {711, 2}}, // Move 218
        {{711, 1}, {711, 2}, {712, 2}, {713, 2}}, // Move 219
        {{714, 1}, {714, 2}, {715, 2}, {716, 300}}, // Move 220
    },
    {
        {{730, 1}, {730, 2}}, // Move 231
        {{730, 1}, {730, 2}}, // Move 232
        {{731, 1}, {731, 3}, {732, 3}}, // Move 233
        {{733, 1}, {733, 3}, {734, 300}}, // Move 234
    },
};

struct NormalHit { unsigned frame, stun, stop, height_mask; };

// The serialized hit descriptor's first word and its +0x10 stop value.
// Conditional descriptors are retained only when first encountered at a frame.
constexpr NormalHit filia_normal_hits[26][6] = {
    {{8, 16, 7, 3}}, // Move 97
    {{10, 26, 9, 3}}, // Move 100
    {{9, 31, 11, 3}}, // Move 102
    {{8, 25, 7, 3}}, // Move 99
    {{7, 22, 9, 3}}, // Move 101
    {{16, 26, 14, 3}}, // Move 103
    {{6, 14, 7, 3}}, // Move 104
    {{11, 28, 9, 3}}, // Move 107
    {{10, 24, 9, 3}}, // Move 109
    {{5, 15, 7, 2}}, // Move 106
    {{7, 20, 4, 2}, {11, 20, 4, 3}, {15, 20, 4, 3}, {19, 20, 4, 3}, {23, 20, 9, 3}}, // Move 108
    {{9, 14, 11, 2}}, // Move 110
    {{7, 17, 7, 1}}, // Move 111
    {{17, 17, 9, 1}, {22, 16, 4, 1}}, // Move 113
    {{7, 23, 11, 1}}, // Move 115
    {{7, 17, 7, 1}}, // Move 112
    {{7, 17, 7, 1}, {14, 20, 7, 3}}, // Move 114
    {{9, 28, 11, 3}}, // Move 116
    {{4, 16, 7, 3}}, // Move 98
    {{6, 14, 7, 3}}, // Move 105
    {{4, 16, 9, 3}, {9, 40, 8, 3}}, // Move 161
    {{4, 16, 10, 3}, {9, 40, 8, 3}}, // Move 162
    {{6, 40, 12, 3}, {8, 40, 11, 3}, {13, 40, 9, 3}}, // Move 163
    {{11, 12, 2, 3}, {13, 32, 18, 3}}, // Moves 154 and 156
    {{11, 12, 2, 3}, {13, 19, 16, 3}}, // Moves 155 and 157
    {}, // Ringlet Psych
};

// Grounded-target velocities from the hit expressions in FOIT moves 102, 103,
// 109 and 110. Vertical values here use the preview's positive-up coordinates.
struct HeavyReaction {
    enum Type { none, knockback, launch, sweep } type = none;
    float horizontal = 0, upward = 0;
};
constexpr HeavyReaction filia_heavy_reaction(int move) {
    switch (move) {
    case 2: return {HeavyReaction::knockback, 28, 6};
    case 5: return {HeavyReaction::knockback, 18, 30};
    case 8: return {HeavyReaction::launch, -15, 6};
    case 11: return {HeavyReaction::sweep, 16, 13};
    default: return {};
    }
}

// Poses from FOIT moves 284/286 (flight), 247/248 (back floor impact),
// and 272/274 followed by 259/261/262 (front fall and impact).
constexpr NormalFrame filia_knockback[] = {{769, 8}, {770, 4}, {771, 4}, {772, 4}};
constexpr NormalFrame filia_back_down[] = {{744, 2}, {745, 2}, {735, 4}, {736, 4}, {737, 4}, {738, 40}};
constexpr NormalFrame filia_front_fall[] = {{757, 3}, {758, 3}, {759, 3}, {760, 3}, {761, 3}};
constexpr NormalFrame filia_front_down[] = {{752, 5}, {753, 2}, {739, 4}, {740, 4}, {741, 40}};

}
