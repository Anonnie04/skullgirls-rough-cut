#pragma once

#include "cerebella.hpp"

namespace skullgirls {

struct SquiglyPlayback : CerebellaPlayback {
    static constexpr unsigned stand = 21, crouch = 25, normals = 113;
    static constexpr float gravity=1.5f;
    static constexpr unsigned first_attack = 115, last_attack = 297;

    SquiglyPlayback() {
        move=20; // CreatePersistentObjects precedes the playable idle.
        mirror_script_positions=true;
        for (auto& bit : state.bits) bit = false;
        // Nonzero defaults from Squigly's original 0xb1-float root FOIT block.
        // Initialize the complete block so optional unknown operands do not
        // accidentally inherit another character's values.
        for(unsigned slot=0;slot<0xb1;++slot)state.variables[slot]=0.0f;
        state.variables[0x26]=240.0f;
        state.variables[0x36]=gravity;state.variables[0x37]=1.5f;
        state.variables[0x38]=1.0f;state.variables[0x3e]=2.0f;
        state.variables[0x40]=state.variables[0x41]=14300.0f;
        state.variables[0x44]=1.0f;state.variables[0x45]=1.35f;
        state.variables[0x53]=1.0f;state.variables[0x65]=40.0f;
        state.variables[0x6a]=1.0f;state.variables[0x75]=0.75f;
        state.variables[0x76]=0.3f;state.variables[0x7c]=1.0f;
        state.variables[0x80]=5.0f;state.variables[0x81]=15.0f;
        for(auto slot:{0xa2,0xa3,0xa4,0xa5,0xa6})state.variables[slot]=1.0f;
        state.variables[0x1b]=1.0f; // Point body; the assist adapter sets this to zero.
        throw_damage_slot = 0x73;
    }

    void start(unsigned target) {
        // Divekick authors its hit on the immediate final startup record;
        // Divekick_Dive inherits that descriptor instead of defining a new one.
        const bool dive=target==272 && (move==271 || move==272);
        const auto* previous_hit=hit;const bool resolved=hit_resolved;
        CerebellaPlayback::start(target);state.variables[0x09]=0.0f;state.variables[0x36]=gravity;
        if(dive){hit=previous_hit;hit_resolved=resolved;}
    }

    bool attacking() const { return move >= first_attack && move <= last_attack; }
    unsigned grab_successor() const { return move==144?145u:move==152?154u:0; }

    void animate(const FoitMoveTable& moves) {
        if(move<moves.categories.size())state.move_categories=moves.categories[move];
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
                    return (move>=57&&move<=74&&id>=65&&id<=101) ||
                        (move>=115&&move<=139&&id>=115&&id<=139) ||
                        (move>=161&&move<=279&&id>=161&&id<=279);
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
