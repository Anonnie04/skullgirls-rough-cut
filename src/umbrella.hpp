#pragma once

#include "cerebella.hpp"

namespace skullgirls {

struct UmbrellaPlayback : CerebellaPlayback {
    static constexpr unsigned stand = 21, crouch = 22, normals = 122;
    static constexpr float gravity=1.5f;
    static constexpr unsigned first_attack = 124, last_attack = 262;

    // TODO(UMBRELLA): finish bubble pop/capture and Bubble Jump callbacks,
    // s.HK catch eligibility, projectile reflection and Hunger HUD presentation.
    FoitScriptState opponent;
    bool native_hit_pending=false;

    UmbrellaPlayback() {
        move=20;mirror_script_positions=true;
        for(auto& bit:state.bits)bit=false;
        for(unsigned slot=0;slot<0xb1;++slot)state.variables[slot]=0.0f;
        state.variables[0x26]=240.0f;
        state.variables[0x36]=1.5f;
        state.variables[0x37]=1.5f;
        state.variables[0x38]=1.0f;
        state.variables[0x3e]=2.0f;
        state.variables[0x40]=14300.0f;
        state.variables[0x41]=14300.0f;
        state.variables[0x43]=-1.0f;
        state.variables[0x44]=1.0f;
        state.variables[0x45]=1.350000023841858f;
        state.variables[0x52]=1.5f;
        state.variables[0x53]=1.0f;
        state.variables[0x6a]=444.0f;
        state.variables[0x75]=0.75f;
        state.variables[0x76]=0.30000001192092896f;
        state.variables[0x7c]=1.0f;
        state.variables[0x7d]=1.0f;
        state.variables[0x80]=4.0f;
        state.variables[0x81]=15.0f;
        state.variables[0xa2]=1.0f;
        state.variables[0xa3]=1.0f;
        state.variables[0xa4]=1.0f;
        state.variables[0xa5]=1.0f;
        state.variables[0xa6]=1.0f;
        state.variables[0x1b]=1.0f;throw_damage_slot=0x73;defer_throw_damage=true;
    }
    float meter_remainder=0.0f;
    void sample(unsigned direction,unsigned pressed,unsigned buttons,bool grounded,unsigned meter) {
        CerebellaPlayback::sample(direction,pressed,buttons,grounded,meter);
        const float exact=std::clamp(float(meter)*100.0f+meter_remainder,0.0f,500.0f);
        state.variables[0x42]=state.team_variables[0x42]=exact;
    }
    void publish_meter(unsigned& meter) {
        const float exact=std::clamp(state.team_variables[0x42].value_or(float(meter)*100.0f),0.0f,500.0f);
        meter=unsigned(exact/100.0f);meter_remainder=exact-float(meter)*100.0f;
        state.variables[0x42]=exact;
    }
    void start(unsigned target) {
        const auto* previous_hit=hit;const bool resolved=hit_resolved;
        const bool carry=(move==209&&target==210)||(move==210&&target==210)||(move==141&&target==142)||(move==142&&target==143)||
            (move==171&&(target==172||target==173))||(move==172&&target==172)||
            (move==173&&target==173)||(move==218&&target==219)||(move==219&&target==219)||
            (move==513&&target==514);
        CerebellaPlayback::start(target);state.variables[0x09]=captured_target?1.0f:0.0f;
        state.variables[0x36]=gravity;state.variables[0xa4]=state.variables[0xa5]=1.0f;
        if(carry){hit=previous_hit;hit_resolved=resolved;}
    }

    FoitExpression bind_opponent(FoitExpression e) const {
        // A root fighter is its own character owner. Its native table also
        // uses owner operands in shared root/child routines (Feeding Time).
        for(auto* item:{&e.left,&e.right,&e.destination}) {
            if(item->flags==9)item->flags=1;
            if(item->flags==0x28)item->flags=0x20;
        }
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
            if(item->flags==0x41&&captured_target&&item->value<target_state.variables.size())
                if(const auto number=target_state.variables[item->value]) {
                    item->flags=0;std::memcpy(&item->value,&*number,sizeof(float));
                }
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
        for(const auto& e:script) {
            if(e.opcode==0x2b&&e.left.flags==1){native_hit_pending=false;continue;}
            // Grab scripts can write a victim slot and read it again in the
            // same record. Bind each expression against the live target.
            CerebellaPlayback::apply({bind_opponent(e)});
        }
    }

    void flush_capture_damage() {
        // The native damage trigger consumes the completed record, including
        // conditional damage variants, rather than a partially bound expression.
        if(captured_target&&value(0x73)!=0&&value(0x74)>0) {
            throw_damage+=unsigned(value(0x74));state.variables[0x73]=0.0f;
        }
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
        flush_capture_damage();
        if(value(0x3a)!=0)ticks=0;
    }

    bool attacking() const { return (move>=124&&move<=262)||(move>=510&&move<=516); }
    unsigned grab_successor() const { return move==186?187u:move==196?198u:(move==226&&hit&&hit->damage==0)?227u:((move==209||move==210)&&hit&&hit->damage==0)?211u:move==233||move==234?238u:move==143?144u:move==249?250u:0; }

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
        const bool continuing=ticks!=0&&record!=0;
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations.at(move);
            if (record < clip.size()) {
                load_frame(clip[record++]);

            }
            else {
                const auto target = transition(moves, move, [&](unsigned id) {
                    return (move>=60&&move<=83&&id>=62&&id<=119)||
                        (move>=124&&move<=262&&id>=124&&id<=262)||
                        (move>=510&&move<=516&&id>=510&&id<=516);
                });
                start(target != move ? target : moves.successors.at(move));
                if(move<moves.categories.size())state.move_categories=moves.categories[move];
            }
        }
        if(continuing) {
            const auto& frame=moves.animations.at(move).at(record-1);
            const auto live=[&](const auto& data) {
                if(data.script_flags&1)return;
                const auto saved=state;
                apply(data.script);
                if(data.script_flags&2) {
                    for(unsigned i=0;i<state.variables.size();++i)
                        if(i!=0x5d&&i!=0x54&&i!=0x56&&i!=0x5b)state.variables[i]=saved.variables[i];
                } else if(data.script_flags&(4|8))
                    for(auto i:{0xa0,0xa1,0x9e,0x9f})state.variables[i]=saved.variables[i];
                if(data.clear_hit)hit=nullptr;
                if(data.hit&&hit!=&*data.hit){hit=&*data.hit;hit_resolved=false;}
                FoitAnimationFrame events{};events.script=data.script;
                visual.enter(events,state,inputs,facing_right);
                for(const auto& spawn:data.spawns)visual.pending.push_back({0,&spawn,0,0,1,false,state});
            };
            // FUN_0049a7f0 evaluates unflagged records and their conditions
            // every tick; entry-only scripts keep their authored flag.
            live(frame);
            for(const auto& variant:frame.variants)if(matches(variant.condition))live(variant);
        }
        if (ticks) --ticks;
        state.variables[0x3a] = 0.0f;
        apply(moves.scripts.at(move));
        apply_tick(moves);
        flush_capture_damage();
        state.variables[0x3a] = 0.0f;
        age = std::min(age + 1, static_cast<unsigned>(inputs.size()));
    }
};

}
