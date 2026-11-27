#pragma once

#include "cerebella.hpp"

namespace skullgirls {

struct DoublePlayback : CerebellaPlayback {
    static constexpr unsigned stand = 21, crouch = 22, normals = 109;
    static constexpr float gravity=1.8333332538604736f;
    static constexpr unsigned first_attack = 111, last_attack = 235;

    FoitScriptState opponent;

    DoublePlayback() {
        move=20; // CreatePersistentObjects precedes the playable idle.
        mirror_script_positions=true;
        for (auto& bit : state.bits) bit = false;
        // Nonzero defaults from Double's original 0xb1-float root FOIT block.
        // Initialize the complete block so optional unknown operands do not
        // accidentally inherit another character's values.
        for(unsigned slot=0;slot<0xb1;++slot)state.variables[slot]=0.0f;
        state.variables[0x26]=240.0f;
        state.variables[0x36]=gravity;state.variables[0x37]=1.5f;
        state.variables[0x38]=1.0f;state.variables[0x3e]=2.0f;
        state.variables[0x40]=state.variables[0x41]=14300.0f;
        state.variables[0x44]=1.0f;state.variables[0x45]=1.35f;
        state.variables[0x53]=1.0f;
        state.variables[0x75]=0.75f;
        state.variables[0x76]=0.3f;state.variables[0x7c]=1.0f;
        state.variables[0x80]=8.0f;state.variables[0x81]=15.0f;
        for(auto slot:{0xa2,0xa3,0xa4,0xa5,0xa6})state.variables[slot]=1.0f;
        state.variables[0x1b]=1.0f; // Point body; the assist adapter sets this to zero.
        throw_damage_slot = 0x73;
    }

    void start(unsigned target) {
        const bool carry=((target==484&&(move==483||move==484)) || (target==486&&(move==485||move==486))) || (target==153&&(move==152||move==153)) || (target>=165&&target<=167&&move>=164&&move<=167) ||
            (target==172&&(move==171||move==172)) || (target==175&&(move==174||move==175)) ||
            (target==178&&(move==177||move==178));
        const auto* previous_hit=hit;const bool resolved=hit_resolved;
        CerebellaPlayback::start(target);state.variables[0x09]=0.0f;state.variables[0x36]=gravity;
        // FUN_0049cdf0 restores both render scales on every move change.
        state.variables[0xa4]=state.variables[0xa5]=1.0f;
        if(carry){hit=previous_hit;hit_resolved=resolved;}
    }

    // The car's return animation reads the opponent's position/facing directly.
    // Bind only native opponent reads; capture writes remain in the shared host.
    void apply(const std::vector<FoitExpression>& script) {
        if(script.empty())return;
        auto bound=script;
        for(auto& expression:bound)for(auto* item:{&expression.left,&expression.right})
            if(item->flags==0x11 && item->value<opponent.variables.size())
                if(const auto number=opponent.variables[item->value]) {
                    item->flags=0;std::memcpy(&item->value,&*number,sizeof(float));
                }
        CerebellaPlayback::apply(bound);
    }

    void load_frame(const FoitAnimationFrame& frame) {
        const auto descriptor=[&](const auto& data) {
            if(data.clear_hit)hit=nullptr;
            if(data.hit){hit=&*data.hit;hit_resolved=false;}
        };
        const auto present=[&](const auto& data) {
            FoitAnimationFrame events{};
            events.script=data.script;events.sounds=data.sounds;
            visual.enter(events,state,inputs,facing_right);
            for(const auto& spawn:data.spawns)visual.pending.push_back({0,&spawn,0,0,1,false,state});
        };
        state.variables[0x3a]=0.0f;apply(frame.script);descriptor(frame);ticks=frame.ticks;
        if(frame.sprites[1]>1)pose=frame.sprites[1];
        present(frame);
        for(const auto& variant:frame.variants) {
            if(!match_foit_condition(variant.condition,inputs,facing_right,state))continue;
            apply(variant.script);descriptor(variant);if(variant.duration)ticks=*variant.duration;
            // Luger sets its own projectile-active bit here. Do not reevaluate
            // that condition after its write and lose the associated spawn.
            present(variant);
        }
        if(value(0x3a)!=0)ticks=0;
    }

    bool attacking() const { return (move >= first_attack && move <= last_attack) || (move>=483&&move<=487); }
    unsigned grab_successor() const { return move==136?137u:move==144?146u:0; }
    static bool supported_entry(unsigned id) { return !(id>=189&&id<=229); }

    void apply_tick(const FoitMoveTable& moves) {
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
    }

    void animate(const FoitMoveTable& moves) {
        if(move<moves.categories.size())state.move_categories=moves.categories[move];
        for(auto slot:{0x80,0x81,0x82,0x89,0x8a}) {
            const auto timer=value(slot);
            if(timer!=0)state.variables[slot]=timer>0?std::max(0.0f,timer-1):std::min(0.0f,timer+1);
        }
        bool entry_tick=false;
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations.at(move);
            if (record < clip.size()) {
                // Rush Hour's contact-driven override starts with aa=-1.
                // Its first record resets aa; evaluate the entry override first.
                if(move==186&&record==0){apply_tick(moves);entry_tick=true;}
                load_frame(clip[record++]);
            }
            else {
                const auto target = transition(moves, move, [&](unsigned id) {
                    return (move>=53&&move<=70&&id>=61&&id<=106) ||
                        (move>=483&&move<=487&&id>=483&&id<=487) ||
                        (move>=111&&move<=131&&id>=111&&id<=131) ||
                        (move>=152&&move<=230&&id>=152&&id<=230&&supported_entry(id));
                });
                start(target != move ? target : moves.successors.at(move));
                if(move<moves.categories.size())state.move_categories=moves.categories[move];
            }
        }
        if (ticks) --ticks;
        state.variables[0x3a] = 0.0f;
        apply(moves.scripts.at(move));
        if(!entry_tick)apply_tick(moves);
        state.variables[0x3a] = 0.0f;
        age = std::min(age + 1, static_cast<unsigned>(inputs.size()));
    }
};

}
