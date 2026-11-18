#pragma once

#include "filia_normals.hpp"

#include <cstddef>

namespace skullgirls {

struct DefenseClip {
    const NormalFrame* frames;
    std::size_t size;

    template <std::size_t N>
    constexpr DefenseClip(const NormalFrame (&records)[N]) : frames(records), size(N) {}

    constexpr unsigned record_tick(std::size_t record) const {
        unsigned ticks = 0;
        for (std::size_t i = 0; i + 1 < record && i < size; ++i) ticks += frames[i].ticks;
        return ticks;
    }

    constexpr unsigned duration() const { return record_tick(size + 1); }

    constexpr unsigned pose_at(unsigned tick, unsigned hold) const {
        for (std::size_t i = 0; i < size; ++i) {
            if (frames[i].sprite) hold = frames[i].sprite;
            if (tick < frames[i].ticks) break;
            tick -= frames[i].ticks;
        }
        return hold;
    }
};

constexpr NormalFrame stand_preblock[] = { // FOIT move 360
    {862, 1}, {862, 2}, {863, 2}, {864, 2}, {865, 2}, {866, 2}, {867, 2},
};

constexpr NormalFrame stand_guard[] = { // FOIT move 361
    {868, 1}, {868, 3}, {869, 3}, {869, 3},
};

constexpr NormalFrame stand_postblock[] = { // FOIT move 363
    {871, 1}, {871, 2}, {872, 2},
};

constexpr NormalFrame crouch_preblock[] = { // FOIT move 365
    {873, 1}, {873, 3}, {874, 3}, {875, 4},
};

constexpr NormalFrame crouch_guard[] = { // FOIT move 366
    {876, 1}, {876, 4}, {877, 4}, {878, 4},
};

constexpr NormalFrame crouch_postblock[] = { // FOIT move 368
    {879, 1}, {879, 2}, {880, 2}, {881, 2},
};

constexpr NormalFrame air_preblock[] = { // FOIT move 369
    {0, 1}, {882, 2}, {883, 3},
};

constexpr NormalFrame air_guard[] = { // FOIT move 370
    {0, 1}, {884, 3}, {885, 3}, {886, 3},
};

constexpr NormalFrame air_postblock[] = { // FOIT move 372
    {0, 1}, {887, 2}, {888, 2},
};

constexpr NormalFrame stand_high_stun[] = { // FOIT move 376
    {889, 1}, {889, 3}, {890, 3}, {891, 3},
};

constexpr NormalFrame stand_mid_stun[] = { // FOIT move 378
    {892, 1}, {892, 3}, {893, 3}, {894, 3},
};

constexpr NormalFrame stand_pushblock[] = { // FOIT move 380
    {0, 1}, {895, 3}, {896, 2}, {897, 8}, {898, 5}, {899, 300},
};

constexpr NormalFrame crouch_low_stun[] = { // FOIT move 385
    {900, 1}, {900, 3}, {901, 3},
};

constexpr NormalFrame crouch_mid_stun[] = { // FOIT move 387
    {902, 1}, {902, 3}, {903, 3},
};

constexpr NormalFrame crouch_pushblock[] = { // FOIT move 389
    {0, 1}, {904, 2}, {905, 2}, {906, 8}, {907, 4}, {908, 200},
};

constexpr NormalFrame air_stun_start[] = { // FOIT move 392
    {0, 1}, {909, 3}, {910, 3},
};

constexpr NormalFrame air_stun_loop[] = { // FOIT move 393
    {0, 1}, {911, 3}, {912, 3}, {913, 3},
};

constexpr NormalFrame air_pushblock[] = { // FOIT move 395
    {0, 1}, {914, 8}, {915, 3}, {916, 3}, {917, 3}, {918, 3}, {919, 3}, {917, 3},
    {918, 3}, {919, 3}, {917, 3}, {918, 3}, {919, 3}, {917, 3}, {918, 3}, {919, 3},
    {917, 3}, {918, 3}, {919, 300},
};

constexpr NormalFrame burst_animation[] = { // FOIT move 406
    {0, 1}, {920, 2}, {921, 2}, {922, 4}, {923, 2}, {0, 1}, {924, 2}, {925, 2},
    {926, 2}, {927, 2}, {928, 3}, {929, 3}, {930, 3}, {929, 3}, {930, 3}, {929, 3},
    {930, 3}, {929, 3}, {930, 3}, {929, 3}, {930, 3}, {931, 5}, {932, 5}, {0, 1},
};

constexpr NormalFrame burst_fall[] = { // FOIT move 408
    {166, 1}, {166, 2}, {167, 3}, {168, 2}, {169, 3},
};

constexpr NormalFrame burst_fall_loop[] = { // FOIT move 409
    {170, 1}, {170, 30},
};

constexpr NormalFrame ground_tech_recovery[] = { // FOIT move 420
    {0, 1}, {953, 3}, {954, 3}, {955, 3}, {956, 3}, {957, 3}, {958, 3},
};

constexpr NormalFrame tech_facedown_forward[] = { // FOIT move 424
    {0, 1}, {959, 4}, {0, 1}, {960, 4}, {0, 1}, {961, 2}, {962, 3}, {963, 2},
    {964, 3}, {965, 2}, {966, 3}, {967, 2}, {0, 1}, {968, 3}, {969, 3}, {970, 4},
    {971, 3},
};

constexpr NormalFrame tech_facedown_back[] = { // FOIT move 428
    {0, 1}, {972, 4}, {0, 1}, {0, 1}, {973, 4}, {0, 1}, {974, 4}, {0, 1},
    {975, 4}, {976, 4}, {977, 2}, {0, 1}, {978, 9}, {979, 2}, {0, 1}, {980, 4},
    {981, 3},
};

constexpr NormalFrame tech_faceup_forward[] = { // FOIT move 432
    {0, 1}, {982, 3}, {0, 1}, {983, 4}, {0, 1}, {984, 5}, {985, 5}, {986, 5},
    {987, 5}, {0, 1}, {988, 4}, {989, 4}, {990, 3},
};

constexpr NormalFrame tech_faceup_back[] = { // FOIT move 436
    {0, 1}, {991, 3}, {0, 1}, {992, 4}, {0, 1}, {993, 5}, {994, 5}, {995, 5},
    {0, 1}, {996, 2}, {0, 1}, {997, 3}, {998, 4}, {999, 3},
};

struct GroundTechMotion {
    DefenseClip roll;
    unsigned launch_record, brake_record;
    float horizontal, upward, gravity;
};

// The forward rolls divide the distance in 0xa9 by the flight ticks in 0xa8.
// Face-down back tech writes -17/+20 directly and divides the lift by five.
constexpr GroundTechMotion ground_tech_motion[2][2] = {
    {{tech_facedown_back, 12, 15, 17, 20, 4}, {tech_facedown_forward, 5, 13, 440.0f / 17, 15, 30.0f / 17}},
    {{tech_faceup_back, 5, 9, 340.0f / 16, 15, 30.0f / 16}, {tech_faceup_forward, 5, 10, 440.0f / 20, 15, 30.0f / 20}},
};

}
