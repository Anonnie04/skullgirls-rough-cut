#pragma once

#include "cerebella.hpp"

namespace skullgirls {

struct ParasoulPlayback : CerebellaPlayback {
    static constexpr unsigned stand = 20, crouch = 21, normals = 110;
    static constexpr unsigned first_attack = 112, last_attack = 229;

    ParasoulPlayback() {
        for (unsigned bit = 22; bit <= 35; ++bit) state.bits[bit] = false;
        state.variables[0x44] = 1.0f;
        state.variables[0x8e] = 0.0f;
        state.variables[0x63] = state.variables[0x64] = 0.0f; // set-tear count and rotating tear slot
        throw_damage_slot = 0x73;
    }

    bool attacking() const { return move >= first_attack && move <= last_attack; }
    unsigned grab_successor() const { return move == 165 ? 167 : move == 174 ? 176 : 0; }

    void animate(const FoitMoveTable& moves) {
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations.at(move);
            if (record < clip.size()) load_frame(clip[record++]);
            else {
                const auto target = transition(moves, move, [&](unsigned id) {
                    return (move >= 51 && move <= 68 && id >= 59 && id <= 86) ||
                        (move == 138 && (id == 139 || id == 140)) || (move == 222 && id == 223);
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
