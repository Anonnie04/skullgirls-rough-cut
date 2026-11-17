#pragma once

#include "filia_normals.hpp"

namespace skullgirls {

// Entry, hold, and two recovery branches from Filia's installed FOIT.
enum HurtGroup { stand_mid, crouch_high, crouch_low, stand_low, air_mid, stand_high };
enum HurtPhase { hurt_entry, hurt_loop, hurt_recovery, hurt_end };
enum class HurtHeight { automatic, high, mid, low };

constexpr HurtGroup filia_ground_hurt(HurtHeight height, bool crouched) {
    if (crouched) return height == HurtHeight::low ? crouch_low : crouch_high;
    if (height == HurtHeight::high) return stand_high;
    return height == HurtHeight::low ? stand_low : stand_mid;
}

// FOIT transitions compare the remaining hitstun (variable 0x35).
constexpr unsigned filia_recovery_start(HurtGroup group) { return group == crouch_low ? 9 : 11; }
constexpr unsigned filia_recovery_end(HurtGroup group) { return group == crouch_low ? 3 : 5; }

constexpr NormalFrame filia_hurt[6][4][4] = {
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
        {{0, 1}, {729, 2}, {730, 3}}, // Moves 229 and 231
        {{730, 1}, {730, 2}}, // Move 232
        {{731, 1}, {731, 3}, {732, 3}}, // Move 233
        {{733, 1}, {733, 3}, {734, 300}}, // Move 234
    },
    {
        {{694, 1}, {694, 2}}, // Move 209
        {{695, 1}, {695, 3}}, // Move 210
        {{696, 1}, {696, 2}, {697, 2}, {698, 2}}, // Move 211
        {{699, 1}, {699, 2}, {700, 2}, {701, 300}}, // Move 212
    },
};

struct NormalHit {
    unsigned frame, stun, stop, height_mask;
    unsigned clear_frame = 0;
    HurtHeight hurt_height = HurtHeight::automatic;

    constexpr HurtHeight reaction_height() const {
        if (hurt_height != HurtHeight::automatic) return hurt_height;
        if (height_mask == 1) return HurtHeight::high;
        return height_mask == 2 ? HurtHeight::low : HurtHeight::mid;
    }
};

// The serialized hit descriptor's first word and its +0x10 stop value.
// Conditional descriptors are retained only when first encountered at a frame.
// Frame numbers are one-based. The FOIT clear flag (FUN_0049a7f0) drops the
// descriptor before collision checks on the indicated frame.
// Written values 4/5/6 in hit expressions target the high/mid/low reactions.
constexpr NormalHit filia_normal_hits[ringlet_spike + 1][6] = {
    {{8, 16, 7, 3, 10, HurtHeight::high}}, // Move 97
    {{10, 26, 9, 3, 11}}, // Move 100
    {{9, 31, 11, 3, 11}}, // Move 102
    {{8, 25, 7, 3, 10, HurtHeight::high}}, // Move 99
    {{7, 22, 9, 3, 8}}, // Move 101
    {{16, 26, 14, 3, 18}}, // Move 103
    {{6, 14, 7, 3, 7}}, // Move 104
    {{11, 28, 9, 3, 12}}, // Move 107
    {{10, 24, 9, 3, 12}}, // Move 109
    {{5, 15, 7, 2, 6}}, // Move 106
    {{7, 20, 4, 2, 9}, {11, 20, 4, 3, 13, HurtHeight::low}, {15, 20, 4, 3, 17, HurtHeight::low},
     {19, 20, 4, 3, 21, HurtHeight::low}, {23, 20, 9, 3, 25, HurtHeight::low}}, // Move 108
    {{9, 14, 11, 2, 18}}, // Move 110
    {{7, 17, 7, 1, 9}}, // Move 111
    {{17, 17, 9, 1, 19}}, // Move 113
    {{7, 23, 11, 1, 11}}, // Move 115
    {{7, 17, 7, 1, 8}}, // Move 112
    {{7, 17, 7, 1, 9}, {14, 20, 7, 3, 16, HurtHeight::mid}}, // Move 114
    {{9, 28, 11, 3, 11}}, // Move 116
    {{4, 16, 7, 3, 6, HurtHeight::high}}, // Move 98
    {{6, 14, 7, 3, 7}}, // Move 105
    // Updo's last hit descriptor is cleared on entry to move 165.
    {{4, 16, 9, 3}, {9, 40, 8, 3, 16}}, // Move 161
    {{4, 16, 10, 3, 9}, {9, 40, 8, 3, 16}}, // Move 162
    {{6, 40, 12, 3, 8}, {8, 40, 11, 3, 13}, {13, 40, 9, 3, 20}}, // Move 163
    // Frame 3 of the Hairball end moves clears the finisher.
    {{11, 12, 2, 3, 12}, {13, 32, 18, 3, 14}}, // Moves 154 and 156
    {{11, 12, 2, 3, 12}, {13, 19, 16, 3, 14}}, // Moves 155 and 157
    {}, // Ringlet Psych
    {}, // Ringlet Spike uses its child object's hit descriptor.
};
constexpr NormalHit ringlet_spike_hit{0, 12, 0, 3};
// The grounded reference capture holds the impact pose for about twelve
// additional 30 fps frames before the launch begins.
constexpr unsigned ringlet_grounded_impact_hold = 24;

// Base damage from the same FOIT hit descriptors, in contact order.
constexpr unsigned filia_hit_damage[ringlet_spike + 1][6] = {
    {300}, {550}, {1000}, {400}, {450}, {1000},
    {200}, {600}, {900}, {200}, {150, 150, 200, 250, 400}, {1100},
    {250}, {250}, {900}, {250}, {200, 350}, {900},
    {200}, {200}, {800, 400}, {950, 400}, {900, 900, 400},
    {300, 400}, {300, 400}, {}, {},
};

// Hit expressions from FOIT moves 102, 103, 109 and 110. Moves 103 and 110
// specify the same forced reaction against grounded and airborne targets.
struct HeavyReaction {
    enum Type { none, knockback, ground_bounce, launch, back_knockdown, sweep } type = none;
    float horizontal = 0, upward = 0;
};
// Child record 6 writes target slots 0x9e = -9, 0x9f = 16 and 0x5d = 2.
// Airborne reaction 2 dispatches to the front fall in move 272.
constexpr HeavyReaction ringlet_spike_reaction{HeavyReaction::sweep, 9, 16};
constexpr HeavyReaction filia_heavy_reaction(int move, bool airborne = false) {
    switch (move) {
    case 2: return airborne ? HeavyReaction{} : HeavyReaction{HeavyReaction::knockback, 6, 28};
    // Move 103 writes horizontal recoil -30 and vertical launch +18.
    case 5: return {HeavyReaction::knockback, 30, 18};
    case 8: return airborne ? HeavyReaction{} : HeavyReaction{HeavyReaction::ground_bounce, 6, -15};
    case 11: return {HeavyReaction::sweep, 13, 16};
    default: return {};
    }
}

// Crouching HP writes reaction 7, which enters 303/304. On floor contact
// 305 halves the downward speed and 306 reverses it before dispatching to 275.
constexpr NormalFrame filia_ground_bounce[] = {{785, 1}, {742, 9}};
constexpr unsigned filia_ground_bounce_ticks = filia_ground_bounce[0].ticks + filia_ground_bounce[1].ticks;
// Angle bands from 275-282, using positive-up angles relative to facing.
constexpr unsigned filia_launch_sprite(float angle) {
    if (angle >= 70 && angle < 110) return 762;
    if ((angle >= 110 && angle < 150) || (angle >= -70 && angle < -30)) return 763;
    if (angle >= 150 || (angle >= -30 && angle < 0)) return 764;
    if (angle < -150 || (angle >= 0 && angle < 30)) return 765;
    if ((angle >= -150 && angle < -130) || (angle >= 30 && angle < 50)) return 766;
    if ((angle >= -130 && angle < -110) || (angle >= 50 && angle < 70)) return 767;
    return 768;
}

// 244-248 play the back impact and rebound before the 252/254/412 recovery.
constexpr NormalFrame filia_back_impact[] = {{0, 2}, {742, 4}, {743, 4}};
constexpr unsigned filia_back_rebound_tick =
    filia_back_impact[0].ticks + filia_back_impact[1].ticks + filia_back_impact[2].ticks;
constexpr NormalFrame filia_back_rebound[] = {
    {743, 2}, {744, 2}, {745, 2}, {735, 4}, {736, 4}, {737, 4}
};
constexpr NormalFrame filia_back_wakeup[] = {
    {746, 4}, {747, 2}, {748, 3}, {749, 2}, {750, 3}, {751, 5},
    {933, 3}, {934, 3}, {935, 3}, {936, 3}, {937, 4}, {938, 7},
    {939, 3}, {940, 4}, {941, 5}, {942, 4}, {943, 4},
};

// FOIT 284/286 and 272/274 supply the flight poses. Both loop states
// branch to 258 on landing, then play the 259/261/262 impact sequence.
constexpr NormalFrame filia_knockback_start[] = {{769, 9}};
constexpr NormalFrame filia_knockback_loop[] = {{770, 4}, {771, 4}, {772, 4}};
constexpr NormalFrame filia_front_fall_start[] = {{757, 4}, {758, 5}};
constexpr NormalFrame filia_front_fall_loop[] = {{759, 3}, {760, 3}, {761, 4}};
constexpr NormalFrame filia_floor_impact[] = {{752, 5}, {753, 2}};
constexpr unsigned filia_floor_rebound_tick = filia_floor_impact[0].ticks + filia_floor_impact[1].ticks;
// Move 259 writes +11 vertical speed and 1.5 gravity on its final record,
// and clamps the stored horizontal speed to ±12 before entering 260/261.
constexpr float filia_floor_rebound_upward = 11.0f;
constexpr float filia_floor_rebound_gravity = 1.5f;
constexpr float filia_floor_rebound_horizontal_limit = 12.0f;
constexpr NormalFrame filia_floor_rebound[] = {{753, 2}, {739, 4}, {740, 4}};
constexpr unsigned filia_floor_down_sprite = 741;

// FOIT 265 holds the floor pose for one tick. Moves 266/267 lead to 414,
// with the last pose held through the one-tick successor 397.
constexpr NormalFrame filia_front_wakeup[] = {
    {filia_floor_down_sprite, 1}, {754, 3}, {755, 2}, {756, 15},
    {944, 3}, {945, 3}, {946, 3}, {947, 3}, {948, 5},
    {949, 7}, {950, 3}, {951, 5}, {952, 6},
};

}
