#pragma once

#include "foit_script.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace skullgirls {

struct FoitVisualRequest {
    unsigned effect = 0;
    const FoitSpawn* spawn = nullptr;
    float x = 0, y = 0, parameter = 0;
    bool attached = false;
    FoitScriptState parent;
};

// Presentation commands execute even in records skipped by the animation's
// immediate-frame flag. The fighter and the effects keep independent clocks.
struct FoitVisualState {
    float freeze = 0;
    int source = -1;
    unsigned next_record = 0;
    unsigned buffered_buttons = 0, buffer_ticks = 0;
    std::vector<FoitVisualRequest> pending;

    void enter(const FoitAnimationFrame& frame, const FoitScriptState& context,
               const FoitInputHistory& inputs, bool facing_right) {
        auto state = context;
        state.variables[0x47] = freeze;
        state.variables[0x6b] = state.variables[0x6c] = state.variables[0x6d] = 0.0f;
        state.variables[0x6e] = 0.0f;
        const auto apply = [&](const auto& script) {
            for (const auto& expression : script) {
                const auto slot = expression.destination.value;
                if (expression.destination.flags == 1 && (slot == 0x47 || (slot >= 0x6b && slot <= 0x6e)))
                    apply_foit_assignment(expression, state);
            }
        };
        const auto spawn = [&](const auto& entries) {
            for (const auto& entry : entries) pending.push_back({0, &entry, 0, 0, 1, false, context});
        };
        apply(frame.script);
        spawn(frame.spawns);
        for (const auto& variant : frame.variants) {
            if (!match_foit_condition(variant.condition, inputs, facing_right, context)) continue;
            apply(variant.script);
            spawn(variant.spawns);
        }
        freeze = state.variables[0x47].value_or(0.0f);
        const auto id = state.variables[0x6b].value_or(0.0f);
        if (id > 0 && std::isfinite(id)) {
            const float parameter = state.variables[0x6e].value_or(0.0f);
            pending.push_back({static_cast<unsigned>(id > 1000 ? id - 1000 : id), nullptr,
                state.variables[0x6c].value_or(0), state.variables[0x6d].value_or(0),
                parameter, id > 1000, context});
        }
    }

    void enter_records(const FoitMoveTable& table, unsigned move, unsigned through,
                       const FoitScriptState& state, const FoitInputHistory& inputs, bool facing) {
        if (source != int(move) || (through != unsigned(-1) && through + 1 < next_record)) {
            source = int(move);
            next_record = 0;
        }
        const auto& frames = table.animations.at(move);
        while (next_record < frames.size() && next_record <= through)
            enter(frames[next_record++], state, inputs, facing);
    }
};

struct FoitEffectPlayback {
    const FoitMoveTable* root = nullptr;
    const FoitMoveTable* table = nullptr;
    unsigned move = 0, record = 0, ticks = 0, pose = 0;
    FoitScriptState state, defaults;
    FoitVisualState visual;
    float x = 0, y = 0, offset_x = 0, offset_y = 0;
    bool facing_right = true, alive = true, generic = false;
    std::size_t side = 0, palette_slot = 0;
    std::uint64_t owner_instance = 0;

    float value(unsigned slot) const { return state.variables[slot].value_or(0.0f); }
    std::uint32_t texture() const {
        const float number = value(0x50);
        std::uint32_t bits;
        std::memcpy(&bits, &number, sizeof(bits));
        return bits;
    }

    void initialize(const FoitMoveTable& definition) {
        table = &definition;
        for (unsigned i = 0; i < state.variables.size(); ++i) state.variables[i] = definition.initial_variables[i];
        for (auto& bit : state.bits) bit = false;
        defaults = state;
    }

    FoitExpression bind(FoitExpression expression, const FoitScriptState& parent) const {
        const auto operand = [&](FoitOperand item) {
            if ((item.flags == 9 || item.flags == 0x11 || item.flags == 0x19) &&
                item.value < parent.variables.size() && parent.variables[item.value]) {
                const float number = *parent.variables[item.value];
                item.flags = 0;
                std::memcpy(&item.value, &number, sizeof(number));
            } else if (item.flags == 0x28 || item.flags == 0x38) {
                float index;
                std::memcpy(&index, &item.value, sizeof(index));
                if (index >= 0 && index < parent.bits.size() && parent.bits[unsigned(index)]) {
                    const float number = *parent.bits[unsigned(index)] ? 1.0f : 0.0f;
                    item.flags = 0;
                    std::memcpy(&item.value, &number, sizeof(number));
                }
            }
            return item;
        };
        expression.left = operand(expression.left);
        expression.right = operand(expression.right);
        if (expression.comparison) expression.destination = operand(expression.destination);
        return expression;
    }

    void apply(const std::vector<FoitExpression>& script, const FoitScriptState& parent) {
        for (const auto& original : script) {
            auto expression = bind(original, parent);
            if (!expression.comparison && expression.opcode == 1 && expression.destination.flags == 1 &&
                expression.destination.value == 0x50 && expression.left.flags == 0x24) {
                float hash;
                std::memcpy(&hash, &expression.left.value, sizeof(hash));
                state.variables[0x50] = hash;
            } else apply_foit_script({expression}, state, defaults);
        }
    }

    void start(unsigned target) {
        move = target;
        record = ticks = 0;
        state.variables[0x36] = defaults.variables[0x36];
        state.variables[0x37] = defaults.variables[0x37];
    }

    bool matches(FoitCondition condition, const FoitScriptState& parent) const {
        for (auto& step : condition.steps)
            for (auto& test : step.tests) test = bind(test, parent);
        return match_foit_condition(condition, FoitInputHistory{}, facing_right, state).has_value();
    }

    void advance(const FoitScriptState& parent, bool owner_active, bool frozen = false) {
        if (!alive || (frozen && !(unsigned(value(0x8f)) & 8u))) return;
        const auto& script = table->scripts[move];
        const bool positioned_by_owner = std::any_of(script.begin(), script.end(), [](const auto& expression) {
            return expression.destination.flags == 1 &&
                (expression.destination.value == 0xa0 || expression.destination.value == 0xa1) &&
                (expression.left.flags == 9 || expression.left.flags == 0x19);
        });
        if (!owner_active && (value(0x39) == 1 || positioned_by_owner)) {
            state.variables[0xb0] = 1.0f;
            const auto end = std::find(table->names.begin(), table->names.end(), "End");
            if (end == table->names.end()) {
                alive = false;
                return;
            }
            const auto target = unsigned(end - table->names.begin());
            if (move != target) start(target);
        }
        if (value(0x39) == 1) {
            x = parent.variables[0xa0].value_or(x) + offset_x;
            y = parent.variables[0xa1].value_or(y) + offset_y;
        }
        state.variables[0x05] = y <= 0 ? 1.0f : 0.0f;
        state.variables[0xa0] = x;
        state.variables[0xa1] = y;
        for (const auto& link : table->transitions[move]) {
            if (link.target != move && matches(link.condition, parent)) {
                start(link.target);
                break;
            }
        }
        unsigned immediate = 0;
        while (alive && !ticks && immediate++ < 64) {
            const auto& clip = table->animations[move];
            if (record == clip.size()) {
                start(table->successors[move]);
                continue;
            }
            const auto& frame = clip[record++];
            state.variables[0x3a] = 0.0f;
            apply(frame.script, parent);
            ticks = frame.ticks;
            if (frame.sprites[1]) pose = frame.sprites[1];
            alive = !frame.destroys_entity;
            for (const auto& variant : frame.variants) {
                if (!matches(variant.condition, parent)) continue;
                apply(variant.script, parent);
                if (variant.duration) ticks = *variant.duration;
                if (variant.destroys_entity) alive = false;
            }
            visual.enter(frame, state, FoitInputHistory{}, facing_right);
            if (value(0x3a) != 0) ticks = 0;
        }
        if (ticks) --ticks;
        apply(table->scripts[move], parent);
        if (const auto& tick_override = table->tick_overrides[move]; tick_override && matches(tick_override->condition, parent)) {
            apply(tick_override->frame.script, parent);
            if (tick_override->frame.destroys_entity) alive = false;
            visual.enter(tick_override->frame, state, FoitInputHistory{}, facing_right);
        }
        x = value(0xa0);
        y = value(0xa1);
        const auto mode = value(0x3b);
        facing_right = value(0x38) > 0;
        if (mode != 4) {
            x += value(0x9e) * (facing_right ? 1.0f : -1.0f);
            y += value(0x9f);
            if (mode == 1 || mode == 2) state.variables[0x9f] = value(0x9f) - value(0x36);
            if (mode == 1 || mode == 3) state.variables[0x9e] = std::copysign(
                std::max(0.0f, std::abs(value(0x9e)) - value(0x37)), value(0x9e));
        }
    }
};

}
