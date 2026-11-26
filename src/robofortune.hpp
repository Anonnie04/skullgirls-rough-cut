#pragma once

#include "cerebella.hpp"

namespace skullgirls {

struct RoboFortunePlayback : CerebellaPlayback {
    static constexpr unsigned stand = 21, crouch = 23, normals = 106;
    static constexpr unsigned first_attack = 108, last_attack = 217;

    RoboFortunePlayback() {
        mirror_script_positions=true;
        for (unsigned bit = 10; bit <= 35; ++bit) state.bits[bit] = false;
        for (auto slot : {0x09,0x0a,0x11,0x12,0x1b,0x1e,0x68,0x69,0x6a,0x7e,0x7f,0x81,0x82,
                          0x89,0x8a,0x90,0x95,0x96,0x98,0xa2,0xa3}) state.variables[slot]=0.0f;
        state.variables[0xa4]=state.variables[0xa5]=state.variables[0xa6]=1.0f;
        state.variables[0x6a]=4.0f; // Free head slots, including the body head.
        state.variables[0xa2]=state.variables[0xa3]=1.0f;
        state.variables[0x40] = state.variables[0x41] = 14300.0f;
        state.variables[0x44] = 1.0f;
        state.variables[0x7c] = state.variables[0x7d] = 1.0f;
        state.variables[0x80] = 8.0f;
        state.variables[0x8e] = 0.0f;
        state.bits[20] = true;
        move = 20;
        throw_damage_slot = 0x73;
    }

    FoitPlaybackState<FoitEffectPlayback> reaction;

    bool attacking() const { return move >= first_attack && move <= last_attack; }
    unsigned grab_successor() const { return move == 145 ? 146u : move == 153 ? 155u : (move == 189 || move == 190) ? 192u : 0; }

    void animate(const FoitMoveTable& moves) {
        if(move<moves.categories.size())state.move_categories=moves.categories[move];
        // FUN_00499980 advances signed engine timers once per unfrozen tick.
        for(auto slot:{0x80,0x81,0x82,0x89,0x8a}) {
            const auto timer=value(slot);
            if(timer!=0)state.variables[slot]=timer>0?std::max(0.0f,timer-1):std::min(0.0f,timer+1);
        }
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations.at(move);
            if (record < clip.size()) load_frame(clip[record++]);
            else {
                const auto target = transition(moves, move, [&](unsigned id) {
                    return (move >= 55 && move <= 72 && id >= 63 && id <= 96) ||
                        (move >= 180 && move <= 209 && id >= 180 && id <= 209);
                });
                start(target != move ? target : moves.successors.at(move));
                if(move<moves.categories.size())state.move_categories=moves.categories[move];
            }
        }
        if (ticks) --ticks;
        state.variables[0x3a] = 0.0f;
        apply(moves.scripts.at(move));
        if (const auto& tick = moves.tick_overrides.at(move); tick) {
            const auto& frame = tick->frame;
            const auto execute = [&](const auto& data) {
                FoitAnimationFrame events{};
                events.script = data.script; events.sounds = data.sounds;
                visual.enter(events, state, inputs, facing_right);
                for (const auto& spawn : data.spawns)
                    visual.pending.push_back({0, &spawn, 0, 0, 1, false, state});
                apply(data.script);
                if (data.clear_hit) hit = nullptr;
                if (data.hit) { hit = &*data.hit; hit_resolved = false; }
            };
            if (match_foit_condition(tick->condition, inputs, facing_right, state)) execute(frame);
            for (const auto& variant : frame.variants) {
                if (!match_foit_condition(variant.condition, inputs, facing_right, state)) continue;
                execute(variant);
            }
        }
        state.variables[0x3a] = 0.0f;
        age = std::min(age + 1, static_cast<unsigned>(inputs.size()));
    }
};

}
