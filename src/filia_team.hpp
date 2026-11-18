#pragma once

#include "filia_normals.hpp"

#include <cstddef>

namespace skullgirls {

constexpr NormalFrame tag_out_taunt[] = { // FOIT move 439
    {0, 1}, {1000, 2}, {0, 1}, {0, 1}, {1001, 3}, {0, 1}, {1002, 3}, {0, 1},
    {1003, 3}, {1004, 3}, {1005, 2}, {1006, 3}, {1007, 6}, {1008, 2}, {0, 1}, {1009, 2},
    {1010, 2}, {1011, 2}, {0, 1},
};

constexpr NormalFrame tag_out_fall[] = { // FOIT move 442
    {1013, 2}, {1014, 3},
};

constexpr NormalFrame tag_in_taunt[] = { // FOIT move 448
    {0, 1}, {1015, 2}, {1016, 2}, {1017, 2}, {1018, 2}, {1019, 2}, {1020, 2}, {1021, 2},
    {1022, 2}, {1023, 2}, {1024, 5}, {1025, 2}, {1026, 2}, {1027, 2}, {1028, 2}, {1029, 5},
    {1030, 2}, {1031, 2}, {1032, 2}, {1033, 2}, {1034, 2}, {1035, 2},
};

constexpr NormalFrame assist_landing[] = { // FOIT move 450
    {0, 1}, {113, 1}, {114, 1},
};

constexpr NormalFrame assist_exit_start[] = { // FOIT move 451
    {119, 1}, {120, 1}, {121, 1}, {0, 1},
};

constexpr NormalFrame assist_exit_rise[] = { // FOIT move 452
    {0, 1}, {134, 4},
};

constexpr unsigned assist_exit_rise_hold = 135; // FOIT move 453

constexpr NormalFrame assist_exit_peak[] = { // FOIT move 454
    {150, 2}, {151, 3}, {152, 2}, {153, 3},
};

constexpr NormalFrame assist_exit_fall[] = { // FOIT move 455
    {176, 1}, {176, 2}, {177, 3}, {178, 2}, {179, 3},
};

constexpr unsigned assist_exit_fall_hold = 180; // FOIT move 456

constexpr NormalFrame assist_exit_land[] = { // FOIT move 457
    {0, 1}, {113, 1}, {114, 1},
};

constexpr NormalFrame snap_out_start[] = { // FOIT move 323
    {769, 4}, {769, 4}, {0, 1},
};

constexpr NormalFrame snap_out_fly[] = { // FOIT move 324
    {770, 2}, {770, 2}, {771, 2}, {771, 2}, {772, 2}, {772, 2},
};

template <std::size_t N>
constexpr unsigned team_animation_ticks(const NormalFrame (&frames)[N]) {
    unsigned ticks = 0;
    for (const auto& frame : frames) ticks += frame.ticks;
    return ticks;
}

template <std::size_t N>
constexpr unsigned team_animation_sprite(const NormalFrame (&frames)[N], unsigned tick, unsigned pose) {
    for (const auto& frame : frames) {
        if (frame.sprite) pose = frame.sprite;
        if (tick < frame.ticks) break;
        tick -= frame.ticks;
    }
    return pose;
}

}
