#pragma once

#include "cerebella.hpp"

namespace skullgirls {

struct PainwheelPlayback : CerebellaPlayback {
    static constexpr unsigned stand = 21, crouch = 22, normals = 109;
    static constexpr unsigned first_attack = 112, last_attack = 247;

    PainwheelPlayback() {
        for (auto& bit : state.bits) bit = false;
        for (auto slot : {0x09, 0x1b, 0x1e, 0x40, 0x58, 0x59, 0x5a, 0x5e, 0x62, 0x63,
                          0x64, 0x65, 0x68, 0x69, 0x7b, 0x80, 0x81, 0x82, 0x89, 0x8a,
                          0x8e, 0x90, 0x95, 0x96, 0x98}) state.variables[slot] = 0.0f;
        state.variables[0x44] = 1.0f;
        state.variables[0x40] = 15500.0f;
        state.variables[0x1b] = 1.0f;
        move = 20; // CreatePersistentObjects precedes the idle.
        throw_damage_slot = 0x73;
    }

    bool attacking() const {
        return (move >= first_attack && move <= 199) || (move >= 222 && move <= last_attack);
    }
    bool flying() const { return move == 200 || move == 201 || (move >= 215 && move <= 219); }
    unsigned grab_successor() const {
        switch (move) {
        case 146: return value(0xa8) > 0 ? 148u : 147u;
        case 159: return 161;
        case 185: case 187: case 189: return move + 1;
        case 191: case 194: case 197: return move + 2;
        case 225: return 227;
        default: return 0;
        }
    }

    void start(unsigned target) {
        const bool rolling = (target == 239 || target == 240) && move >= 238 && move <= 240;
        const auto* active_hit = hit;
        const bool resolved = hit_resolved;
        CerebellaPlayback::start(target);
        // Overdrive authors its hit once, before entering the rolling loops.
        if (rolling) { hit = active_hit; hit_resolved = resolved; }
    }

    void animate(const FoitMoveTable& moves) {
        for (auto slot : {0x80, 0x82, 0x89, 0x8a})
            if (value(slot) > 0) state.variables[slot] = value(slot) - 1;
        const auto internal = transition(moves, move, [&](unsigned id) {
            return ((move==177 || move==237) && (id==179 || id==238)) ||
                (move==149 && id==150) || (move==234 && id==235);
        });
        if (internal != move) start(internal);
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations.at(move);
            if (record < clip.size()) load_frame(clip[record++]);
            else {
                const auto target = transition(moves, move, [&](unsigned id) {
                    return ((move>=52 && move<=70) && id>=61 && id<=108) ||
                        (move==116 && id==118) || (move==129 && id==131) ||
                        ((move==215 || move==216) && id>=217 && id<=219) ||
                        (move==177 && id==179) || (move==237 && id==238) ||
                        (move==149 && id==150) || (move==234 && id==235);

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
