#pragma once

#include <array>
#include <cstddef>

namespace skullgirls {

// Filia's initial FOIT slots 0x36 and 0x80. Ground jumps reload slot 0x80 at takeoff.
constexpr float filia_gravity = 1.5f;
constexpr unsigned filia_air_dash_lockout = 5;

// Filia FOIT moves 88 and 90. Timing omits script records that set variable
// 0x3a and advance again before the engine renders.

struct MovementFrame {
    unsigned sprite;
    unsigned ticks;
    float velocity;
};

constexpr std::array air_dash_forward{
    MovementFrame{190, 4, 17}, MovementFrame{191, 2, 17}, MovementFrame{192, 2, 17},
    MovementFrame{193, 2, 17}, MovementFrame{191, 2, 17}, MovementFrame{192, 2, 17},
    MovementFrame{193, 2, 17}, MovementFrame{191, 2, 17}, MovementFrame{192, 2, 17},
    MovementFrame{193, 2, 17}, MovementFrame{190, 2, 8}};
constexpr std::array air_dash_backward{
    MovementFrame{194, 4, -17}, MovementFrame{195, 2, -17}, MovementFrame{196, 2, -17},
    MovementFrame{197, 2, -17}, MovementFrame{195, 2, -17}, MovementFrame{196, 2, -17},
    MovementFrame{197, 2, -9}, MovementFrame{195, 2, -9}, MovementFrame{196, 2, -9},
    MovementFrame{197, 2, -9}, MovementFrame{195, 2, -9}, MovementFrame{194, 3, -4}};

// Filia FOIT moves 27, 29, 30, 36 and 40. Sprite references are one-based.
// Velocities are relative to the fighter's facing.
constexpr std::array walk_forward{
    MovementFrame{59, 3, 6}, MovementFrame{60, 4, 6}, MovementFrame{61, 3, 6},
    MovementFrame{62, 4, 6}, MovementFrame{63, 3, 6}, MovementFrame{64, 4, 6},
    MovementFrame{65, 3, 6}, MovementFrame{66, 4, 6}, MovementFrame{67, 3, 6},
    MovementFrame{68, 4, 6}, MovementFrame{69, 3, 6}, MovementFrame{70, 4, 6}};
constexpr std::array walk_backward{
    MovementFrame{71, 3, -4.5f}, MovementFrame{72, 3, -4.5f}};
constexpr std::array walk_backward_loop{
    MovementFrame{73, 3, -4.5f}, MovementFrame{74, 3, -4.5f}, MovementFrame{75, 3, -4.5f},
    MovementFrame{76, 3, -4.5f}, MovementFrame{77, 3, -4.5f}, MovementFrame{78, 3, -4.5f},
    MovementFrame{79, 3, -4.5f}, MovementFrame{80, 3, -4.5f}, MovementFrame{81, 3, -4.5f},
    MovementFrame{82, 3, -4.5f}, MovementFrame{83, 3, -4.5f}, MovementFrame{84, 3, -4.5f},
    MovementFrame{85, 3, -4.5f}, MovementFrame{86, 3, -4.5f}, MovementFrame{87, 3, -4.5f},
    MovementFrame{88, 3, -4.5f}, MovementFrame{89, 3, -4.5f}, MovementFrame{90, 3, -4.5f},
    MovementFrame{91, 3, -4.5f}, MovementFrame{92, 3, -4.5f}};
constexpr std::array dash_forward{
    MovementFrame{93, 2, 12}, MovementFrame{94, 2, 15}, MovementFrame{95, 2, 15},
    MovementFrame{96, 2, 15}, MovementFrame{97, 2, 15}, MovementFrame{95, 2, 15},
    MovementFrame{96, 2, 15}, MovementFrame{97, 2, 15}, MovementFrame{95, 2, 15},
    MovementFrame{96, 2, 15}, MovementFrame{94, 4, 14}};
constexpr std::array dash_backward{
    MovementFrame{107, 2, -12}, MovementFrame{108, 2, -12}, MovementFrame{109, 2, -17},
    MovementFrame{110, 2, -17}, MovementFrame{111, 2, -17}, MovementFrame{109, 2, -10},
    MovementFrame{110, 2, -10}, MovementFrame{111, 2, -10}, MovementFrame{109, 2, -10},
    MovementFrame{112, 3, -6}, MovementFrame{112, 3, -2}};

// The held forward dash continues from move 36 into 37, then loops 38.
// Releasing forward in 37/38 enters 39.
constexpr std::array dash_forward_extend{
    MovementFrame{98, 4, 14}};
constexpr std::array dash_forward_loop{
    MovementFrame{99, 4, 14}, MovementFrame{100, 4, 14}, MovementFrame{101, 2, 14},
    MovementFrame{102, 4, 14}, MovementFrame{103, 4, 14}, MovementFrame{104, 4, 14},
    MovementFrame{105, 2, 14}, MovementFrame{106, 4, 14}};
constexpr std::array dash_forward_stop{
    MovementFrame{94, 8, 14}, MovementFrame{93, 4, 14}};

// FOIT 24/25 (standing/crouching turn) and 31 (backwalk stop).
constexpr std::array<unsigned, 14> stand_turn{
    50, 50, 50, 50, 51, 51, 51, 51, 52, 52, 52, 53, 53, 53};
constexpr std::array<unsigned, 15> crouch_turn{
    54, 54, 54, 55, 55, 55, 56, 56, 56, 57, 57, 57, 58, 58, 58};
constexpr std::array<unsigned, 6> backwalk_stop{72, 72, 72, 71, 71, 71};

// Crouch entry/exit (22/23) and the visible jump landing (41-43).
constexpr std::array<unsigned, 16> crouch_enter{
    40, 40, 40, 41, 41, 41, 42, 42, 42, 43, 43, 43, 44, 44, 44, 44};
constexpr std::array<unsigned, 18> crouch_exit{
    45, 45, 45, 46, 46, 46, 47, 47, 47, 47, 48, 48, 48, 48, 49, 49, 49, 49};
constexpr std::array<unsigned, 17> jump_landing{
    114, 114, 114, 115, 115, 115, 116, 116, 116, 116, 117, 117, 117, 118, 118, 118, 118};

// Idle moves 20/21 have an immediate entry record. Retain the two
// conditional records in each loop, where display time depends on the script.
constexpr unsigned stand_idle_ticks = 82;
constexpr std::array<unsigned, 18> crouch_idle_duration{4,5,5,5,4,5,4,5,4,5,5,5,4,5,4,5,4,5};

// Super jump 55 uses ascent 65-70 and descent 74-76/83-85.
constexpr std::array<unsigned, 10> jump_descent{0, 0, 1, 1, 1, 2, 2, 3, 3, 3};
constexpr std::array<unsigned, 11> fall_entry{0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 3};
constexpr std::array<unsigned, 9> high_jump_fall_entry{0, 0, 0, 1, 1, 1, 2, 2, 2};

// FUN_0046c3a0 direction codes, indexed by horizontal and vertical axes.
constexpr unsigned filia_input_direction(int horizontal, int vertical) {
    constexpr unsigned directions[3][3] = {{3, 4, 5}, {2, 8, 6}, {1, 0, 7}};
    return directions[horizontal + 1][vertical + 1];
}

using FiliaDashHistory = std::array<unsigned, 28>;
constexpr unsigned filia_neutral = 8;

// Ground jump dispatchers 44/45 check for the corresponding down direction
// within 17 samples before routing to super jump 55.
constexpr bool filia_super_jump(const FiliaDashHistory& history) {
    const unsigned up = history.front();
    if (up < 5 || up > 7) return false;
    constexpr unsigned down[] = {3, 2, 1};
    for (std::size_t age = 1; age < 17; ++age)
        if (history[age] == down[up - 5]) return true;
    return false;
}

// Direction-only subset of FUN_0046dd60 for the ground and air dash edges.
inline bool filia_double_tap(const FiliaDashHistory& history, int horizontal, bool airborne) {
    if (!horizontal) return false;
    const unsigned tap = horizontal > 0 ? 0 : 4;
    const unsigned opposite = 4 - tap;
    if (history.front() != tap) return false;
    std::size_t step = 1;
    bool previous_step_matched = false;
    const auto matches = [&](std::size_t index, unsigned direction) {
        if (index == 0) return direction == tap;
        if (index == 1 || (index == 3 && !airborne)) return direction == filia_neutral;
        const bool in_tap_direction = direction == tap ||
            (airborne && (direction == (tap == 0 ? 1 : 3) || direction == (tap == 0 ? 7 : 5)));
        // The oldest air-dash step inverts the directional match to find the first tap's edge.
        return index == 3 ? !in_tap_direction : in_tap_direction;
    };
    for (std::size_t age = 1; age < 17; ++age) {
        const auto direction = history[age];
        if (!airborne && direction == opposite) return false;
        if (matches(step, direction)) {
            if (++step == 4) return true;
            previous_step_matched = true;
        } else if (!previous_step_matched && direction == tap) {
            return false;
        } else if (!matches(step - 1, direction)) {
            previous_step_matched = false;
        }
    }
    return false;
}

// FOIT moves 161-163 accept punch on down-forward or immediately after it.
inline bool filia_updo_motion(const FiliaDashHistory& history, bool facing_right) {
    constexpr unsigned mirrored[] = {4, 3, 2, 1, 0, 7, 6, 5, 8};
    const auto direction = [&](std::size_t age) {
        return facing_right ? history[age] : mirrored[history[age]];
    };
    const auto scan = [&](unsigned step, unsigned window) {
        constexpr unsigned pattern[] = {1, 2, 0};
        for (unsigned age = 1; age < window; ++age) {
            const unsigned sample = direction(age);
            if (sample == 4) return false;
            if (sample == pattern[step] && ++step == std::size(pattern)) return true;
        }
        return false;
    };
    return direction(0) != 4 && ((direction(0) == 1 && scan(1, 19)) || scan(0, 22));
}

// FOIT moves 148-153 and 147 use quarter-circle commands with either
// the terminal direction or a separate button step.
inline bool filia_quarter_circle(const FiliaDashHistory& history, bool right,
                                 bool allow_rolling_input) {
    const unsigned end = right ? 0 : 4;
    const unsigned diagonal = right ? 1 : 3;
    const auto scan = [&](unsigned age, unsigned limit, bool rolling, bool held_end) {
        if (rolling) {
            while (age < limit && history[age] < 5) ++age;
            if (age == limit || history[age] > 7) return false;
            ++age;
        }
        if (!held_end) {
            while (age < limit && history[age] != end) ++age;
            if (age == limit) return false;
            ++age;
        }
        while (age < limit && history[age] != diagonal) ++age;
        if (age == limit) return false;
        while (++age < limit)
            if (history[age] == 2) return true;
        return false;
    };
    return (history.front() == end && scan(1, 13, false, true)) ||
        scan(1, 16, false, false) ||
        (allow_rolling_input && scan(1, 28, true, false));
}

// FOIT moves 140/142/144 accept half-circle forward. Punch can share
// the forward sample (25-tick window) or follow it (28-tick window).
inline bool filia_ringlet_half_circle(const FiliaDashHistory& history, bool right) {
    const std::array<unsigned, 5> motion = right ?
        std::array<unsigned, 5>{0, 1, 2, 3, 4} : std::array<unsigned, 5>{4, 3, 2, 1, 0};
    const auto scan = [&](std::size_t step, std::size_t limit) {
        for (std::size_t age = 1; age < limit; ++age)
            if (history[age] == motion[step] && ++step == motion.size()) return true;
        return false;
    };
    return (history.front() == motion.front() && scan(1, 25)) || scan(0, history.size());
}

}
