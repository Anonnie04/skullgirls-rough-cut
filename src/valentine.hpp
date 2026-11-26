#pragma once

#include "cerebella.hpp"

namespace skullgirls {

struct ValentinePlayback : CerebellaPlayback {
    static constexpr unsigned stand = 20, crouch = 21, normals = 117;
    static constexpr unsigned first_attack = 119, last_attack = 255;

    // The host implements movement, normals, ordinary throws, Dead Cross,
    // Savage Bypass, vial loading/tossing, Checkmate Incision and EKG Flatliner.
    // TODO(VALENTINE): poison status consumers, Countervenom, Mortuary Drop,
    // s.Operation and s.AED require additional native engine hooks.
    ValentinePlayback() {
        mirror_script_positions=true;
        for (auto& bit : state.bits) bit = false;
        state.variables[0x44] = 1.0f;
        state.variables[0x8e] = 0.0f;
        for(auto slot:{0x09,0x1b,0x1e,0x62,0x63,0x64,0x65,0x68,0x69,0x6a,0x7e,0x7f,0x81,0x82,0x89,0x8a,0x90,0x95,0x96,0x98})state.variables[slot]=0.0f;
        state.variables[0x1b]=1.0f; // Point body; the assist adapter sets this to zero.
        throw_damage_slot = 0x73;
    }

    void start(unsigned target) { CerebellaPlayback::start(target);state.variables[0x09]=0.0f; }

    bool attacking() const { return move >= first_attack && move <= last_attack; }
    unsigned grab_successor() const { return move == 152 ? 153 : move == 161 ? 163 : move == 188 ? 189 : move == 238 ? 239 : 0; }

    void animate(const FoitMoveTable& moves) {
        if(move<moves.categories.size())state.move_categories=moves.categories[move];
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations.at(move);
            if (record < clip.size()) load_frame(clip[record++]);
            else {
                const auto target = transition(moves, move, [&](unsigned id) {
                    return (move >= 52 && move <= 70 && id >= 60 && id <= 97) ||
                        (move >= 119 && move <= 147 && id >= 119 && id <= 147) ||
                        (move >= 186 && move <= 250 && id >= 186 && id <= 250) ||
                        (move==174 && (id==175 || id==176)) || (move==180 && (id==181 || id==182));
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
