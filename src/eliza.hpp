#pragma once

#include "cerebella.hpp"

namespace skullgirls {

struct ElizaPlayback : CerebellaPlayback {
    static constexpr unsigned stand = 21, crouch = 22, normals = 117;
    static constexpr float gravity=1.6666667461395264f;
    static constexpr unsigned first_attack = 119, last_attack = 318;

    // TODO(ELIZA): recover blood-pool collection and the Vampire mode's
    // owner-health/healing callbacks before claiming full mode parity.
    FoitScriptState opponent;
    float health_remainder=0.0f;
    bool armored_contact=false;
    bool native_hit_pending=false;
    bool sekhmet() const {
        return state.move_categories&&std::find(state.move_categories->begin(),state.move_categories->end(),0xe929c2acu)!=state.move_categories->end();
    }

    ElizaPlayback() {
        move=20;mirror_script_positions=true;
        for(auto& bit:state.bits)bit=false;
        for(unsigned slot=0;slot<0xb1;++slot)state.variables[slot]=0.0f;
        state.variables[0x26]=240.0f;
        state.variables[0x36]=1.66666674614f;
        state.variables[0x37]=1.5f;
        state.variables[0x38]=1.0f;
        state.variables[0x3e]=2.0f;
        state.variables[0x40]=14300.0f;
        state.variables[0x41]=14300.0f;
        state.variables[0x43]=-1.0f;
        state.variables[0x44]=1.0f;
        state.variables[0x45]=1.35000002384f;
        state.variables[0x52]=1.5f;
        state.variables[0x53]=1.0f;
        state.variables[0x75]=0.75f;
        state.variables[0x76]=0.300000011921f;
        state.variables[0x7d]=1.0f;
        state.variables[0x80]=8.0f;
        state.variables[0x81]=15.0f;
        state.variables[0xa2]=1.0f;
        state.variables[0xa3]=1.0f;
        state.variables[0xa4]=1.0f;
        state.variables[0xa5]=1.0f;
        state.variables[0xa6]=1.0f;
        state.variables[0x1b]=1.0f;throw_damage_slot=0x73;
    }
    void start(unsigned target) {
        const auto* previous_hit=hit;const bool resolved=hit_resolved;
        const bool carry=(target==183&&(move>=180&&move<=182)) ||
            (target==186&&(move==184||move==185)) || (target==196&&move==195) ||
            (target==569&&(move==568||move==569));
        CerebellaPlayback::start(target);state.variables[0x09]=captured_target?1.0f:0.0f;
        state.variables[0x36]=gravity;state.variables[0xa4]=state.variables[0xa5]=1.0f;
        if(carry){hit=previous_hit;hit_resolved=resolved;}
    }

    FoitExpression bind_opponent(FoitExpression e) const {
        // FUN_00491ba0 / expression 0x2a tests the engine's pending-hit
        // pointer; 0x2b consumes it. Keep this host context out of FOIT slots.
        if(e.opcode==0x2a&&e.left.flags==1) {
            e.opcode=e.comparison?0x12:1;
            e.left={0,native_hit_pending?0x3f800000u:0u};e.right={0,0};
            if(e.comparison)e.destination={0,0x3f800000};
        }
        if((e.opcode==0x20||e.opcode==0x21||e.opcode==0x26||e.opcode==0x27)&&e.left.flags==0x11&&e.left.value==0x27&&opponent.move_categories) {
            const bool comparison=e.opcode==0x20||e.opcode==0x21;
            const auto hash=comparison?e.destination:e.right;
            bool found=std::find(opponent.move_categories->begin(),opponent.move_categories->end(),hash.value)!=opponent.move_categories->end();
            if(e.opcode==0x21||e.opcode==0x27)found=!found;
            e.left={0,found?0x3f800000u:0u};e.right={0,0};
            if(comparison){e.opcode=0x12;e.destination={0,0x3f800000};}else e.opcode=1;
        }
        for(auto* item:{&e.left,&e.right,&e.destination}) {
            if(item==&e.destination&&!e.comparison)continue;
            if(item->flags==0x11&&item->value<opponent.variables.size())
                if(const auto number=opponent.variables[item->value]) {
                    item->flags=0;std::memcpy(&item->value,&*number,sizeof(float));
                }
        }
        return e;
    }
    bool matches(FoitCondition condition,unsigned oldest=63) const {
        for(auto& step:condition.steps)for(auto& test:step.tests)test=bind_opponent(test);
        return match_foit_condition(condition,inputs,facing_right,state,oldest).has_value();
    }
    template<class Accept>
    unsigned transition(const FoitMoveTable& moves,unsigned source,Accept accept,unsigned oldest=63) const {
        for(const auto& link:moves.transitions[source])
            if(link.target!=source&&accept(link.target)&&matches(link.condition,oldest))return link.target;
        return source;
    }
    void apply(const std::vector<FoitExpression>& script) {
        std::vector<FoitExpression> bound;
        for(const auto& e:script) {
            if(e.opcode==0x2b&&e.left.flags==1){native_hit_pending=false;continue;}
            bound.push_back(bind_opponent(e));
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
            if(!matches(variant.condition))continue;
            apply(variant.script);descriptor(variant);if(variant.duration)ticks=*variant.duration;
            // Evaluate each conditional record once before its own state writes.
            present(variant);
        }
        if(value(0x3a)!=0)ticks=0;
    }

    bool attacking() const { return (move>=119&&move<=258) || (move>=293&&move<=299) || (move>=314&&move<=318) || (move>=568&&move<=570); }
    unsigned grab_successor() const { return move==154?155u:move==162?164u:move==196?198u:move==243?247u:move==255||move==256?257u:0; }
    static bool supported_entry(unsigned id) { return id<=318; }

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
            if (matches(tick->condition)) execute(frame);
            for (const auto& variant : frame.variants) {
                if (!matches(variant.condition)) continue;
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
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations.at(move);
            if (record < clip.size()) {
                load_frame(clip[record++]);
            }
            else {
                const auto target = transition(moves, move, [&](unsigned id) {
                    return (move>=54&&move<=71&&id>=62&&id<=107) ||
                        (move>=119&&move<=149&&id>=119&&id<=149) ||
                        (move>=173&&move<=299&&id>=173&&id<=299) ||
                        (move>=164&&move<=167&&id>=164&&id<=167) ||
                        (move>=568&&move<=570&&id>=568&&id<=570);
                });
                start(target != move ? target : moves.successors.at(move));
                if(move<moves.categories.size())state.move_categories=moves.categories[move];
            }
        }
        if (ticks) --ticks;
        state.variables[0x3a] = 0.0f;
        apply(moves.scripts.at(move));
        apply_tick(moves);
        state.variables[0x3a] = 0.0f;
        age = std::min(age + 1, static_cast<unsigned>(inputs.size()));
    }
};

}
