#pragma once

#include <array>

namespace skullgirls {

// Filia FOIT moves 88 and 90. Zero sprite references retain the last pose.

struct MovementFrame {
    unsigned sprite;
    unsigned ticks;
    float velocity;
};

constexpr std::array air_dash_forward{
    MovementFrame{190, 4, 17}, MovementFrame{191, 2, 17}, MovementFrame{192, 2, 17},
    MovementFrame{193, 2, 17}, MovementFrame{191, 2, 17}, MovementFrame{192, 2, 17},
    MovementFrame{193, 2, 17}, MovementFrame{191, 2, 17}, MovementFrame{192, 2, 17},
    MovementFrame{193, 2, 17}, MovementFrame{190, 2, 8}, MovementFrame{190, 1, 0}};
constexpr std::array air_dash_backward{
    MovementFrame{194, 4, -17}, MovementFrame{195, 2, -17}, MovementFrame{196, 2, -17},
    MovementFrame{197, 2, -17}, MovementFrame{195, 2, -17}, MovementFrame{196, 2, -17},
    MovementFrame{197, 2, -9}, MovementFrame{195, 2, -9}, MovementFrame{196, 2, -9},
    MovementFrame{197, 2, -9}, MovementFrame{195, 2, -9}, MovementFrame{194, 3, -4},
    MovementFrame{194, 2, 0}};

// Filia FOIT moves 27, 29, 30, 36 and 40. Sprite references are one-based.
// The preview folds script-only ticks into adjacent poses. Velocities
// are relative to the fighter's facing.
constexpr std::array walk_forward{
    MovementFrame{59, 4, 6}, MovementFrame{60, 4, 6}, MovementFrame{61, 3, 6},
    MovementFrame{62, 4, 6}, MovementFrame{63, 3, 6}, MovementFrame{64, 4, 6},
    MovementFrame{65, 3, 6}, MovementFrame{66, 5, 6}, MovementFrame{67, 3, 6},
    MovementFrame{68, 5, 6}, MovementFrame{69, 3, 6}, MovementFrame{70, 4, 6}};
constexpr std::array walk_backward{
    MovementFrame{71, 4, -4.5f}, MovementFrame{72, 4, -4.5f}};
constexpr std::array walk_backward_loop{
    MovementFrame{73, 4, -4.5f}, MovementFrame{74, 3, -4.5f}, MovementFrame{75, 3, -4.5f},
    MovementFrame{76, 3, -4.5f}, MovementFrame{77, 4, -4.5f}, MovementFrame{78, 3, -4.5f},
    MovementFrame{79, 3, -4.5f}, MovementFrame{80, 3, -4.5f}, MovementFrame{81, 3, -4.5f},
    MovementFrame{82, 3, -4.5f}, MovementFrame{83, 3, -4.5f}, MovementFrame{84, 3, -4.5f},
    MovementFrame{85, 3, -4.5f}, MovementFrame{86, 3, -4.5f}, MovementFrame{87, 4, -4.5f},
    MovementFrame{88, 3, -4.5f}, MovementFrame{89, 3, -4.5f}, MovementFrame{90, 3, -4.5f},
    MovementFrame{91, 3, -4.5f}, MovementFrame{92, 3, -4.5f}};
constexpr std::array dash_forward{
    MovementFrame{93, 4, 12}, MovementFrame{94, 2, 15}, MovementFrame{95, 2, 15},
    MovementFrame{96, 2, 15}, MovementFrame{97, 2, 15}, MovementFrame{95, 2, 15},
    MovementFrame{96, 2, 15}, MovementFrame{97, 2, 15}, MovementFrame{95, 2, 15},
    MovementFrame{96, 4, 15}, MovementFrame{94, 4, 14}};
constexpr std::array dash_backward{
    MovementFrame{107, 4, -12}, MovementFrame{108, 2, -12}, MovementFrame{109, 2, -17},
    MovementFrame{110, 2, -17}, MovementFrame{111, 2, -17}, MovementFrame{109, 2, -10},
    MovementFrame{110, 2, -10}, MovementFrame{111, 2, -10}, MovementFrame{109, 2, -10},
    MovementFrame{112, 3, -6}, MovementFrame{112, 3, -2}};

// FUN_0046c3a0 direction codes, indexed by horizontal and vertical axes.
constexpr unsigned filia_input_direction(int horizontal, int vertical) {
    constexpr unsigned directions[3][3] = {{3, 4, 5}, {2, 8, 6}, {1, 0, 7}};
    return directions[horizontal + 1][vertical + 1];
}

using FiliaDashHistory = std::array<unsigned, 28>;
constexpr unsigned filia_neutral = 8;

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
        return direction == tap ||
            (airborne && (direction == (tap == 0 ? 1 : 3) || direction == (tap == 0 ? 7 : 5)));
    };
    for (std::size_t age = 1; age < 17; ++age) {
        const auto direction = history[age];
        if (!airborne && direction == opposite) return false;
        const bool air_hold_complete = !airborne || step != 3 || matches(step, history[age + 1]);
        if (matches(step, direction) && air_hold_complete) {
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

}
