#pragma once

#include "filia_defense.hpp"

namespace skullgirls {

constexpr NormalFrame slide_start[] = { // FOIT move 295
    {769, 4}, {769, 4}, {0, 1},
};

constexpr NormalFrame slide_flight[] = { // FOIT move 296
    {770, 2}, {770, 2}, {771, 2}, {771, 2}, {772, 2}, {772, 2},
};

constexpr NormalFrame slide_loop[] = { // FOIT move 298
    {0, 1}, {775, 2}, {776, 2}, {777, 2}, {778, 2},
};

constexpr NormalFrame slide_end[] = { // FOIT move 302
    {779, 3}, {780, 3}, {781, 3}, {782, 9}, {783, 5}, {784, 5}, {0, 1},
};

constexpr NormalFrame floorbounce_flight[] = { // FOIT move 304
    {785, 2}, {786, 2}, {787, 2},
};

constexpr NormalFrame ceiling_start[] = { // FOIT move 307
    {0, 1}, {788, 1}, {788, 1}, {0, 1},
};

constexpr NormalFrame ceiling_rise[] = { // FOIT move 308
    {0, 1}, {789, 3}, {790, 3}, {791, 3},
};

constexpr NormalFrame ceiling_impact[] = { // FOIT move 309
    {0, 1}, {792, 5}, {793, 5},
};

constexpr NormalFrame ceiling_fall[] = { // FOIT move 310
    {785, 3}, {786, 3}, {787, 3},
};

constexpr NormalFrame ceiling_land[] = { // FOIT move 311
    {0, 1}, {742, 9},
};

constexpr NormalFrame crumple_start[] = { // FOIT move 312
    {794, 1}, {794, 4}, {0, 1}, {795, 4}, {796, 4}, {0, 1}, {797, 4},
};

constexpr NormalFrame crumple_crouch[] = { // FOIT move 313
    {798, 1}, {0, 1}, {798, 2}, {799, 2}, {800, 2}, {801, 4}, {802, 4}, {803, 4},
    {804, 4}, {805, 4}, {806, 4}, {807, 4}, {0, 1}, {808, 4}, {0, 1}, {809, 4},
};

constexpr NormalFrame crumple_fall[] = { // FOIT move 314
    {810, 1}, {0, 1}, {810, 4}, {811, 4}, {812, 4}, {813, 4}, {814, 4}, {815, 4},
};

constexpr NormalFrame stagger_loop[] = { // FOIT move 317
    {0, 1}, {816, 4}, {0, 1}, {817, 3}, {818, 4}, {819, 3}, {0, 1}, {820, 4},
    {821, 3},
};

constexpr NormalFrame stagger_end[] = { // FOIT move 318
    {0, 1}, {822, 4}, {823, 4}, {824, 4}, {825, 4},
};

constexpr NormalFrame freeze_loop[] = { // FOIT move 320
    {0, 1}, {826, 99},
};

constexpr NormalFrame freeze_end[] = { // FOIT move 321
    {0, 1}, {826, 1},
};

// FUN_004978c0 uses the absolute 0x6f selector, minus one, in Throw_Stun.
constexpr unsigned throw_victim_poses[] = {
    702, 694, 710, 827, 752, 753, 742, 743, 828, 829, 830, 831,
    832, 833, 834, 835, 836, 837, 838, 839, 840, 841, 842, 843,
    844, 845, 846, 847, 848, 849, 850, 851, 736, 772, 852, 852,
    853, 854, 695, 855, 856, 857, 858, 754, 756, 859, 859, 860,
    861, 816, 817, 818, 819, 820, 821, 822, 823, 824, 825,
};

constexpr unsigned throw_victim_pose(unsigned selector) {
    return throw_victim_poses[selector - 1];
}

constexpr unsigned air_throw_victim_pose(unsigned throw_pose) {
    switch (throw_pose) {
    case 445: return throw_victim_pose(21);
    case 446: case 447: case 448: case 449: case 450:
        return throw_victim_pose(throw_pose - 436);
    case 451: return throw_victim_pose(14);
    case 454: return throw_victim_pose(12);
    default: return throw_victim_pose(1);
    }
}

}
