#pragma once

#include "filia_movement.hpp"
#include "foit_script.hpp"
#include "foit_effects.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace skullgirls {

// The Windows Cerebella move table has its own indices and default physics slots.
struct CerebellaPlayback {
    static constexpr unsigned stand = 20, crouch = 21, normals = 96;
    static constexpr unsigned first_attack = 98, last_attack = 238;
    static constexpr float gravity = 1.6666667461395264f;

    unsigned move = stand, record = 0, ticks = 0, age = 0, pose = 2, held = 0;
    bool facing_right = true, hit_resolved = false, reacting = false;
    bool captured_target = false, release_target = false;
    bool mirror_script_positions = false;
    unsigned throw_damage = 0;
    unsigned throw_damage_slot = 0x78;
    bool defer_throw_damage = false;
    FoitScriptState target_state = [] {
        FoitScriptState value;
        for (auto& slot : value.variables) slot = 0.0f;
        value.variables[0x38] = -1.0f;
        value.variables[0x53] = 1.0f;
        return value;
    }();
    const FoitHit* hit = nullptr;
    FoitInputHistory inputs;
    FoitScriptState state = defaults();
    FoitVisualState visual;

    static const FoitScriptState& defaults() {
        static const auto slots = [] {
            FoitScriptState value;
            for (auto index : {0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x17, 0x23, 0x25, 0x35,
                               0x3a, 0x3b, 0x3e, 0x42, 0x7e, 0x7f, 0x85, 0x9e, 0x9f, 0xa8,
                               0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0}) value.variables[index] = 0.0f;
            for (unsigned i = 0x6b; i <= 0x78; ++i) value.variables[i] = 0.0f;
            for (auto index : {0x47, 0x48, 0x53, 0x5d, 0x5f, 0x60, 0x61, 0x84, 0x86, 0x88, 0x8b, 0xa0, 0xa1})
                value.variables[index] = 0.0f;
            value.variables[0x53] = 1.0f;
            value.variables[0x36] = gravity;
            value.variables[0x37] = 1.5f;
            value.variables[0x38] = 1.0f;
            value.variables[0x7c] = 1.0f;
            value.variables[0x7d] = 0.0f;
            value.variables[0x80] = 2.0f;
            value.variables[0x81] = 15.0f;
            for (auto index : {0, 5, 12, 13, 17, 19, 21, 24, 32, 64}) value.bits[index] = false;
            return value;
        }();
        return slots;
    }

    bool attacking() const { return move >= first_attack && move <= last_attack; }

    unsigned grab_successor() const {
        switch (move) {
        case 134: case 136: return 139;
        case 135: case 137: return 140;
        case 147: return 149;
        case 157: return 162;
        case 166: return 167;
        case 173: return 174;
        case 178: return 179;
        case 209: return 210;
        case 218: return 219;
        default: return 0;
        }
    }

    float value(unsigned slot) const { return state.variables[slot].value_or(0.0f); }

    void start(unsigned target) {
        move = target;
        record = ticks = age = 0;
        hit = nullptr;
        hit_resolved = false;
        // FUN_0049cdf0 runs for conditional links and default successors,
        // including a move's link back to itself.
        state.variables[0x01] = state.variables[0x02] = state.variables[0x03] = 0.0f;
        state.variables[0x36] = defaults().variables[0x36];
        state.variables[0x37] = defaults().variables[0x37];
        state.variables[0x6f] = state.variables[0x84] = 0.0f;
        state.variables[0x85] = 1.0f;
    }

    void report_contact(bool blocked) {
        hit_resolved = true;
        auto& counter = state.variables[blocked ? 0x02 : 0x01];
        *counter += 1.0f;
        state.variables[0x03] = value(0x03) + 1.0f;
    }

    void sample(unsigned direction, unsigned pressed, unsigned buttons, bool grounded, unsigned meter) {
        std::move_backward(inputs.begin(), inputs.end() - 1, inputs.end());
        inputs.front() = foit_input_sample(direction, pressed, buttons, held & ~buttons);
        held = buttons | pressed;
        state.variables[0x05] = grounded ? 1.0f : 0.0f;
        state.variables[0x3e] = grounded ? 2.0f : 1.0f;
        state.variables[0x42] = float(meter) * 100.0f;
        state.team_variables[0x42] = float(meter) * 100.0f;
        state.variables[0x3a] = 0.0f;
    }

    bool showstopper_motion() const {
        int previous = -1, turn = 0;
        for (std::size_t i = 0; i < 30; ++i) {
            const int direction = inputs[i].direction;
            if (direction >= 8 || direction == previous) continue;
            if (previous >= 0) {
                const int step = (previous - direction + 12) % 8 - 4;
                if (std::abs(step) > 2 || (turn && (turn > 0) != (step > 0))) return false;
                turn += step;
                if (std::abs(turn) >= 6) return true;
            }
            previous = direction;
        }
        return false;
    }

    template<class Accept>
    unsigned transition(const FoitMoveTable& moves, unsigned source, Accept accept,
                        unsigned oldest = FoitInputHistory{}.size() - 1) const {
        for (const auto& link : moves.transitions[source]) {
            if (link.target != source && accept(link.target) &&
                match_foit_condition(link.condition, inputs, facing_right, state, oldest)) return link.target;
        }
        return source;
    }

    void load_frame(const FoitAnimationFrame& frame) {
        const auto descriptor = [&](const auto& data) {
            if (data.clear_hit) hit = nullptr;
            if (data.hit) {
                hit = &*data.hit;
                hit_resolved = false;
            }
        };
        state.variables[0x3a] = 0.0f;
        apply(frame.script);
        descriptor(frame);
        ticks = frame.ticks;
        if (frame.sprites[1] > 1) pose = frame.sprites[1];
        for (const auto& variant : frame.variants) {
            if (!match_foit_condition(variant.condition, inputs, facing_right, state)) continue;
            apply(variant.script);
            descriptor(variant);
            if (variant.duration) ticks = *variant.duration;
        }
        if (value(0x3a) != 0) ticks = 0;
        visual.enter(frame, state, inputs, facing_right);
    }

    void apply(const std::vector<FoitExpression>& script) {
        const float previous_facing = value(0x38);
        if(!mirror_script_positions)apply_foit_script(script,state,defaults());
        else for(const auto& expression:script) {
            const float previous_x=value(0xa0);
            apply_foit_script({expression},state,defaults());
            if(mirror_script_positions && value(0x38)<0 && expression.destination.flags==1 &&
               expression.destination.value==0xa0 && expression.left.flags==1 && expression.left.value==0xa0 &&
               (expression.opcode==2 || expression.opcode==3))state.variables[0xa0]=previous_x-(value(0xa0)-previous_x);
        }
        for (auto expression : script) {
            if (expression.destination.flags != 0x28) continue;
            expression.destination.flags = 0x20;
            apply_foit_assignment(expression, state);
        }
        if (value(0x38) != previous_facing) {
            facing_right = value(0x38) > 0;
            state.variables[0x9e] = -value(0x9e);
        }
        if (!captured_target) return;
        for (const auto& original : script) {
            if (original.comparison || original.destination.flags != 0x41) continue;
            auto expression = original;
            const auto operand = [&](FoitOperand item) {
                if (item.flags == 0x41) item.flags = 1;
                else if (item.flags == 1 && item.value < state.variables.size() && state.variables[item.value]) {
                    const float number = *state.variables[item.value];
                    item.flags = 0;
                    std::memcpy(&item.value, &number, sizeof(number));
                }
                return item;
            };
            expression.destination.flags = 1;
            expression.left = operand(expression.left);
            expression.right = operand(expression.right);
            apply_foit_assignment(expression, target_state);
        }
        if (!defer_throw_damage && value(throw_damage_slot) != 0 && value(0x74) > 0) {
            throw_damage += static_cast<unsigned>(value(0x74));
            state.variables[throw_damage_slot] = 0.0f;
        }
        release_target = target_state.variables[0xb0].value_or(0.0f) != 0;
    }

    // Finish loaded records before reading their successor. The bound also keeps
    // asset-authored immediate cycles from stalling a Training tick.
    void animate(const FoitMoveTable& moves, bool follow_successors = true) {
        if (move == 558 && value(0xa9) <= 0) start(559);
        unsigned passes = 0;
        while (!ticks && passes++ < 32) {
            const auto& clip = moves.animations[move];
            if (record < clip.size()) {
                load_frame(clip[record++]);
            } else if (follow_successors) {
                // The jump dispatch records are immediate. Select their directional link
                // before following the neutral successor in the same update.
                const auto target = transition(moves, move, [&](unsigned id) {
                    return (move == 55 || move == 57 || move == 60 || move == 94 || move == 95) &&
                        id >= 61 && id <= 93;
                });
                const auto successor = move == 127 && (held & 4u) ? 128u : moves.successors[move];
                start(target != move ? target : successor);
            } else {
                return;
            }
        }
        if (ticks) --ticks;
        apply(moves.scripts[move]);
        state.variables[0x3a] = 0.0f;
        age = std::min(age + 1, static_cast<unsigned>(inputs.size()));
    }

    void integrate(float& x, float& height, float& velocity, float left, float right) {
        const auto mode = value(0x3b);
        if (mode == 4) return;
        const float horizontal = value(0x9e);
        x = std::clamp(x + (facing_right ? horizontal : -horizontal), left, right);
        height = std::max(0.0f, height + value(0x9f));
        velocity = value(0x9f);
        if (mode == 1 || mode == 2) velocity -= value(0x36);
        if (height == 0 && velocity < 0) velocity = 0;
        state.variables[0x9f] = velocity;
        if (mode == 1 || mode == 3) {
            const float friction = std::max(0.0f, value(0x37));
            state.variables[0x9e] = std::copysign(std::max(0.0f, std::abs(horizontal) - friction), horizontal);
        }
    }
};

}
