#pragma once

#include "cerebella.hpp"

namespace skullgirls {

struct MsFortunePlayback : CerebellaPlayback {
    static constexpr unsigned stand = 21, crouch = 23, normals = 105;
    static constexpr unsigned first_attack = 107, last_attack = 218;

    MsFortunePlayback() {
        for (unsigned bit = 10; bit <= 35; ++bit) state.bits[bit] = false;
        state.variables[0x44] = 1.0f;
        state.variables[0x8e] = 0.0f;
        state.bits[20] = true;
        move = 20;
        throw_damage_slot = 0x73;
    }

    bool attacking() const { return move >= first_attack && move <= last_attack; }
    bool recalling_head = false;
    unsigned grab_successor() const { return move == 139 ? (value(0xb0) ? 141u : 140u) : move == 148 ? 150u : 0; }

    void animate(const FoitMoveTable& moves) {
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations.at(move);
            if (record < clip.size()) load_frame(clip[record++]);
            else {
                const auto target = transition(moves, move, [&](unsigned id) {
                    return (move >= 55 && move <= 72 && id >= 63 && id <= 84) ||
                        (move == 110 && id == 111) || (move == 116 && id == 117) ||
                        (move == 157 && id == 164) || (move == 161 && (id == 162 || id == 163)) ||
                        (move == 206 && (id == 207 || id == 208)) || (move == 211 && id == 212);
                });
                start(target != move ? target : moves.successors.at(move));
            }
        }
        if (ticks) --ticks;
        state.variables[0x3a] = 0.0f;
        apply(moves.scripts.at(move));
        if (const auto& tick = moves.tick_overrides.at(move); tick) {
            const auto& frame = tick->frame;
            const auto execute = [&](const auto& data) {
                FoitAnimationFrame events{};
                events.script = data.script;
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
