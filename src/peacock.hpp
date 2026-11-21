#pragma once

#include "cerebella.hpp"

namespace skullgirls {

// Shared FOIT record/script mechanics; Peacock keeps her own move indices.
struct PeacockPlayback : CerebellaPlayback {
    static constexpr unsigned stand = 20, crouch = 21, normals = 118;
    static constexpr unsigned first_attack = 120, last_attack = 219;

    bool attacking() const { return move >= first_attack && move <= last_attack; }

    PeacockPlayback() {
        // Entity-presence flags gate item drop, Lenny, and each George variant.
        for (unsigned bit = 22; bit <= 27; ++bit) state.bits[bit] = false;
        state.variables[0x44] = 1.0f;
        throw_damage_slot = 0x73;
    }

    unsigned spent_meter = 0;

    unsigned grab_successor() const { return move == 147 ? 148 : move == 157 ? 159 : 0; }

    void animate(const FoitMoveTable& moves) {
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations.at(move);
            if (record < clip.size()) load_frame(clip[record++]);
            else {
                const auto target = transition(moves, move, [&](unsigned id) {
                    return ((move == 52 || move == 53 || move == 64 || move == 66 || move == 69) &&
                        id >= 60 && id <= 87) || (move == 204 && id == 205) ||
                        (move == 209 && id == 210) || (move == 149 && id == 208);
                });
                if (move == 149 && target == 208) spent_meter += 3;
                start(target != move ? target : moves.successors.at(move));
            }
        }
        if (ticks) --ticks;
        state.variables[0x3a] = 0.0f;
        apply(moves.scripts.at(move));
        if (const auto& tick = moves.tick_overrides.at(move); tick &&
            match_foit_condition(tick->condition, inputs, facing_right, state)) {
            const auto& frame = tick->frame;
            apply(frame.script);
            if (frame.clear_hit) hit = nullptr;
            if (frame.hit) { hit = &*frame.hit; hit_resolved = false; }
            for (const auto& variant : frame.variants) {
                if (!match_foit_condition(variant.condition, inputs, facing_right, state)) continue;
                apply(variant.script);
                if (variant.clear_hit) hit = nullptr;
                if (variant.hit) { hit = &*variant.hit; hit_resolved = false; }
            }
            visual.enter(frame, state, inputs, facing_right);
        }
        state.variables[0x3a] = 0.0f;
        age = std::min(age + 1, static_cast<unsigned>(inputs.size()));
    }
};

}
