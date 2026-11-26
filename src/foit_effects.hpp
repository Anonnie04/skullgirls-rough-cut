#pragma once

#include <memory>

#include "foit_script.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace skullgirls {

// A live timeline contains several entity contexts. Keep that storage off the
// Windows stack while preserving value semantics for Training save states.
template<class T> class FoitPlaybackState {
    std::unique_ptr<T> value;
public:
    FoitPlaybackState() = default;
    FoitPlaybackState(const FoitPlaybackState& other) : value(other ? std::make_unique<T>(*other) : nullptr) {}
    FoitPlaybackState(FoitPlaybackState&&) noexcept = default;
    FoitPlaybackState& operator=(const FoitPlaybackState& other) {
        if (this != &other) value = other ? std::make_unique<T>(*other) : nullptr;
        return *this;
    }
    FoitPlaybackState& operator=(FoitPlaybackState&&) noexcept = default;
    explicit operator bool() const { return bool(value); }
    T& operator*() { return *value; }
    const T& operator*() const { return *value; }
    T* operator->() { return value.get(); }
    const T* operator->() const { return value.get(); }
    T& emplace() { value = std::make_unique<T>(); return *value; }
    void reset() { value.reset(); }
};

struct FoitVisualRequest {
    unsigned effect = 0;
    const FoitSpawn* spawn = nullptr;
    float x = 0, y = 0, parameter = 0;
    bool attached = false;
    FoitScriptState parent;
    std::optional<FoitScriptState> owner;
};

// Presentation commands execute even in records skipped by the animation's
// immediate-frame flag. The fighter and the effects keep independent clocks.
struct FoitVisualState {
    float freeze = 0;
    float super_mode = 0;
    int source = -1;
    unsigned next_record = 0;
    unsigned buffered_buttons = 0, buffer_ticks = 0;
    std::vector<FoitVisualRequest> pending;
    std::vector<std::pair<std::uint32_t, FoitScriptState>> pending_sounds;
    void sound(const std::vector<std::uint32_t>& ids, const FoitScriptState& context) {
        for (auto id : ids) if (pending_sounds.size() < 128) pending_sounds.emplace_back(id, context);
    }

    void enter(const FoitAnimationFrame& frame, const FoitScriptState& context,
               const FoitInputHistory& inputs, bool facing_right) {
        auto state = context;
        state.variables[0x47] = freeze;
        state.variables[0x48] = super_mode;
        state.variables[0x6b] = state.variables[0x6c] = state.variables[0x6d] = 0.0f;
        state.variables[0x6e] = 0.0f;
        const auto apply = [&](const auto& script) {
            for (const auto& expression : script) {
                const auto slot = expression.destination.value;
                if (expression.destination.flags == 1 && (slot == 0x47 || slot == 0x48 || (slot >= 0x6b && slot <= 0x6e)))
                    apply_foit_assignment(expression, state);
            }
        };
        const auto spawn = [&](const auto& entries) {
            for (const auto& entry : entries) pending.push_back({0, &entry, 0, 0, 1, false, context});
        };
        sound(frame.sounds, context);
        apply(frame.script);
        spawn(frame.spawns);
        for (const auto& variant : frame.variants) {
            if (!match_foit_condition(variant.condition, inputs, facing_right, context)) continue;
            sound(variant.sounds, context);
            apply(variant.script);
            spawn(variant.spawns);
        }
        freeze = state.variables[0x47].value_or(0.0f);
        super_mode = state.variables[0x48].value_or(0.0f);
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

// Tracks one running entity's move, pose, variables, and visual requests.
struct FoitEffectPlayback {
    const FoitMoveTable* root = nullptr;
    const FoitMoveTable* table = nullptr;
    unsigned move = 0, record = 0, ticks = 0, pose = 0;
    FoitScriptState state, defaults;
    FoitVisualState visual;
    float x = 0, y = 0, offset_x = 0, offset_y = 0;
    bool facing_right = true, alive = true, generic = false;
    bool floor_collision = false;
    bool preserve_world_velocity_on_turn = false;
    bool persistent_owner = false;
    bool independent_owner = false;
    bool initialize_before_links = false;
    // Round presentation uses the original live record evaluator. Combat's
    // character adapters still own their input/collision and movement clocks.
    bool authored_timeline = false;
    bool combat_entity = false;
    unsigned record_duration = 0;
    FoitInputHistory inputs;
    std::optional<bool> input_facing;
    std::size_t side = 0, palette_slot = 0;
    std::uint64_t owner_instance = 0;
    std::uint64_t entity_id = 0, spawner_id = 0;
    std::optional<FoitScriptState> spawner;
    std::optional<std::array<float, 2>> attachment_position;
    const FoitHit* hit = nullptr;
    bool hit_resolved = false;
    unsigned hit_stop = 0;
    FoitScriptState opponent;
    FoitScriptState capture_state;
    bool captured_target = false;
    unsigned capture_damage = 0;
    std::vector<FoitExpression> parent_writes;
    std::vector<FoitExpression> spawner_writes;
    std::uint32_t random_state = 0x9e3779b9u;

    void descriptor(const FoitAnimationFrame& frame) {
        if (frame.clear_hit) hit = nullptr;
        if (frame.hit) { hit = &*frame.hit; hit_resolved = false; }
    }

    void report_contact(bool blocked) {
        hit_resolved = true;
        const unsigned slot = blocked ? 2 : 1;
        state.variables[slot] = value(slot) + 1;
        state.variables[3] = value(3) + 1;
    }

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

    // Resolve reads from related entities before passing the operation to the local evaluator.
    FoitExpression bind(FoitExpression expression, const FoitScriptState& parent) const {
        const bool category_test=expression.comparison && (expression.opcode==0x20 || expression.opcode==0x21);
        const bool category_value=!expression.comparison && (expression.opcode==0x26 || expression.opcode==0x27);
        if((category_test || category_value) && expression.left.value==0x27) {
            const auto flags=expression.left.flags;
            const auto* source=flags==9?&parent:flags==0x91?(spawner?&*spawner:&parent):
                flags==0x11?&opponent:flags==1?&state:nullptr;
            const auto hash=category_test?expression.destination:expression.right;
            if(source && source->move_categories && hash.flags==0x24) {
                const auto& tags=*source->move_categories;
                bool result=std::find(tags.begin(),tags.end(),hash.value)!=tags.end();
                if(expression.opcode==0x21 || expression.opcode==0x27)result=!result;
                const float number=result?1.0f:0.0f;std::uint32_t bits;std::memcpy(&bits,&number,4);
                expression.left={0,bits};expression.right={0,0};
                if(category_test){expression.opcode=0x12;expression.destination={0,0x3f800000};}
                else expression.opcode=1;
            }
        }
        const auto operand = [&](FoitOperand item) {
            if (captured_target && item.flags == 0x41 && item.value < capture_state.variables.size() && capture_state.variables[item.value]) {
                const float number = *capture_state.variables[item.value];
                item.flags = 0;
                std::memcpy(&item.value, &number, sizeof(number));
                return item;
            }
            const auto& context = item.flags == 0x11 ? opponent : item.flags == 0x91 && spawner ? *spawner : parent;
            if ((item.flags == 9 || item.flags == 0x11 || item.flags == 0x19 || item.flags == 0x91) &&
                item.value < context.variables.size() && context.variables[item.value]) {
                const float number = *context.variables[item.value];
                item.flags = 0;
                std::memcpy(&item.value, &number, sizeof(number));
            } else if (item.flags == 0x28 || item.flags == 0x38 || item.flags == 0xb0) {
                float index;
                std::memcpy(&index, &item.value, sizeof(index));
                const auto& context = item.flags == 0xb0 && spawner ? *spawner : parent;
                if (index >= 0 && index < context.bits.size() && context.bits[unsigned(index)]) {
                    const float number = *context.bits[unsigned(index)] ? 1.0f : 0.0f;
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

    void apply(const std::vector<FoitExpression>& script, FoitScriptState parent) {
        const auto previous_facing=value(0x38);
        for (const auto& original : script) {
            auto expression = bind(original, parent);
            for (auto* operand : {&expression.left, &expression.right}) {
                if (operand->flags != 4) continue;
                float limit;
                std::memcpy(&limit, &operand->value, sizeof(limit));
                random_state ^= random_state << 13;
                random_state ^= random_state >> 17;
                random_state ^= random_state << 5;
                const float number = std::trunc(limit * float(random_state & 0xffffffu) / 16777216.0f);
                operand->flags = 0;
                std::memcpy(&operand->value, &number, sizeof(number));
            }
            if (captured_target && !expression.comparison && expression.destination.flags == 0x41) {
                for (auto* operand : {&expression.left, &expression.right}) {
                    if (operand->flags != 1 || operand->value >= state.variables.size() || !state.variables[operand->value]) continue;
                    const float number = *state.variables[operand->value];
                    operand->flags = 0;
                    std::memcpy(&operand->value, &number, sizeof(number));
                }
                expression.destination.flags = 1;
                apply_foit_assignment(expression, capture_state);
                continue;
            }
            if (!expression.comparison && (expression.destination.flags == 9 || expression.destination.flags == 0x91 || expression.destination.flags == 0x28 || expression.destination.flags == 0xb0)) {
                // Resolve child operands now; the caller commits these owner handshakes
                // before advancing the next child or collecting a nested spawn.
                for (auto* operand : {&expression.left, &expression.right}) {
                    if (operand->flags == 1 && operand->value < state.variables.size() && state.variables[operand->value]) {
                        const float number = *state.variables[operand->value];
                        operand->flags = 0;
                        std::memcpy(&operand->value, &number, sizeof(number));
                    }
                }
                const bool to_spawner = (expression.destination.flags == 0x91 || expression.destination.flags == 0xb0) && spawner.has_value();
                expression.destination.flags = (expression.destination.flags == 0x28 || expression.destination.flags == 0xb0) ? 0x20 : 1;
                (to_spawner ? spawner_writes : parent_writes).push_back(expression);
                apply_foit_assignment(expression,to_spawner?*spawner:parent);
                continue;
            }
            if (!expression.comparison && expression.opcode == 1 && expression.destination.flags == 1 &&
                expression.destination.value == 0x50 && expression.left.flags == 0x24) {
                float hash;
                std::memcpy(&hash, &expression.left.value, sizeof(hash));
                state.variables[0x50] = hash;
            } else {
                const float previous_x = value(0xa0);
                apply_foit_script({expression}, state, defaults);
                if (!facing_right && expression.destination.flags == 1 && expression.destination.value == 0xa0 &&
                    expression.left.flags == 1 && expression.left.value == 0xa0 &&
                    (expression.opcode == 2 || expression.opcode == 3))
                    state.variables[0xa0] = previous_x - (value(0xa0) - previous_x);
            }
        }
        // Same facing-change velocity remap used by CerebellaPlayback::apply.
        // Squigly's tail head faces its owner while extending away from them.
        if(preserve_world_velocity_on_turn && value(0x38)!=previous_facing) {
            state.variables[0x9e]=-value(0x9e);facing_right=value(0x38)>0;
        }
        if (captured_target && value(0x73) != 0 && value(0x74) > 0) {
            capture_damage += static_cast<unsigned>(value(0x74));
            state.variables[0x73] = 0.0f;
        }
    }

    void start(unsigned target) {
        if(persistent_owner && target!=move && (target==40 || target==43 || target==44 || target==48 || target==49 || target==51)) {
            state.variables[1]=state.variables[2]=state.variables[3]=0.0f;
            hit=nullptr;hit_resolved=false;
        }
        move = target;
        if(table && move<table->categories.size())state.move_categories=table->categories[move];
        record = ticks = 0;
        record_duration = 0;
        state.variables[0x36] = defaults.variables[0x36];
        state.variables[0x37] = defaults.variables[0x37];
        if (authored_timeline || combat_entity) {
            // FUN_0049cdf0 restores render scale on every move change. In
            // particular, an invisible intro proxy must yield to visible idle.
            for (auto i : {0x01, 0x02, 0x03, 0x6f, 0x84, 0x51}) state.variables[i] = 0.0f;
            state.variables[0x85] = 1.0f;
            for (auto i : {0xa4, 0xa5, 0x50, 0x52}) state.variables[i] = defaults.variables[i];
        }
    }

    bool matches(FoitCondition condition, const FoitScriptState& parent) const {
        if(persistent_owner)for(auto& step:condition.steps)
            step.tests.erase(std::remove_if(step.tests.begin(),step.tests.end(),[](const auto& test){
                return (test.opcode==0x20 || test.opcode==0x21) && test.left.flags==9 && test.left.value==0x27;
            }),step.tests.end());
        for (auto& step : condition.steps)
            for (auto& test : step.tests) test = bind(test, parent);
        return match_foit_condition(condition, inputs, input_facing.value_or(facing_right), state).has_value();
    }

    void advance_authored(FoitScriptState parent) {
        if(move<table->categories.size())state.move_categories=table->categories[move];
        // FUN_00499980 tests links before advancing the record clock;
        // FUN_0049a7f0 reevaluates conditional record data every tick.
        if (table==root) {
            state.variables[0xa0]=x;
            state.variables[0xa1]=y;
            state.variables[0x05]=y<=0?1.0f:0.0f;
        }
        // A newly created or restarted entity enters its first record before
        // expiry links inspect values initialized by that record.
        if(!initialize_before_links || record || ticks) for (const auto& link : table->transitions[move]) {
            if (link.target != move && matches(link.condition, parent)) { start(link.target); break; }
        }
        if (table!=root && floor_collision && y <= 0 && value(0x9f) < 0 && (value(0x3b) == 1 || value(0x3b) == 2))
            state.variables[0x9f] = 0.0f;
        if (value(0x39) == 1) {
            const auto& source = spawner ? *spawner : parent;
            const std::array<float, 2> position{source.variables[0xa0].value_or(x), source.variables[0xa1].value_or(y)};
            if (attachment_position) {
                x += position[0] - (*attachment_position)[0];
                y += position[1] - (*attachment_position)[1];
            }
            attachment_position = position;
        }
        state.variables[0xa0] = x;
        state.variables[0xa1] = y;
        state.variables[0x05] = y <= 0 ? 1.0f : 0.0f;
        unsigned immediate = 0;
        do {
            const bool entering = ticks == 0;
            if (entering) {
                if (record == table->animations[move].size()) {
                    auto successor = table->successors[move];
                    for (const auto& link : table->transitions[move])
                        if (link.target != move && matches(link.condition, parent)) { successor = link.target; break; }
                    start(successor);
                    continue;
                }
                const auto& frame = table->animations[move][record++];
                ticks = record_duration = frame.ticks;
                if (frame.sprites[1]) pose = frame.sprites[1]; // 0 retains, 1 hides.
                descriptor(frame);
            }
            state.variables[0x3a] = 0.0f;
            const auto run = [&](const auto& script) {
                const auto first = parent_writes.size(), first_spawner = spawner_writes.size();
                apply(script, parent);
                for (auto i = first; i < parent_writes.size(); ++i) apply_foit_assignment(parent_writes[i], parent);
                if (spawner) for (auto i = first_spawner; i < spawner_writes.size(); ++i)
                    apply_foit_assignment(spawner_writes[i], *spawner);
            };
            run(table->scripts[move]);
            const auto execute = [&](const auto& data, bool fresh) {
                const auto saved = state;
                if (!(data.script_flags & 1) || fresh) run(data.script);
                if (data.clear_hit) hit = nullptr;
                if (data.hit && (fresh || hit != &*data.hit)) { hit = &*data.hit; hit_resolved = false; }
                // Temporary expression modes restore the same slots as the
                // original evaluator after computing this tick's presentation.
                if (data.script_flags & 2) {
                    for (unsigned i = 0; i < state.variables.size(); ++i)
                        if (i != 0x5d && i != 0x54 && i != 0x56 && i != 0x5b) state.variables[i] = saved.variables[i];
                } else if (data.script_flags & (4 | 8)) {
                    for (auto i : {0xa0, 0xa1, 0x9e, 0x9f}) state.variables[i] = saved.variables[i];
                }
                if (fresh) visual.sound(data.sounds, state);
                if (fresh || combat_entity) {
                    for (const auto& spawn : data.spawns) {
                        FoitVisualRequest request{0, &spawn, 0, 0, 1, false, state};
                        request.owner = parent;
                        visual.pending.push_back(std::move(request));
                    }
                }
                if (data.destroys_entity) alive = false;
            };
            const auto& frame = table->animations[move][record - 1];
            execute(frame, entering);
            unsigned duration = frame.ticks;
            for (const auto& variant : frame.variants) {
                if (!matches(variant.condition, parent)) continue;
                execute(variant, entering);
                if (variant.duration) duration = *variant.duration;
            }
            if (const auto& tick = table->tick_overrides[move]; tick) {
                if (matches(tick->condition, parent)) execute(tick->frame, true);
                for (const auto& variant : tick->frame.variants)
                    if (matches(variant.condition, parent)) execute(variant, true);
            }
            if (duration != record_duration) {
                ticks = unsigned(std::max(0, int(ticks) + int(duration) - int(record_duration)));
                record_duration = duration;
            }
            // Generic effect requests are transient engine slots, independent
            // of the frame's remaining time.
            FoitAnimationFrame events{};
            for (auto i : {0x47, 0x48, 0x6b, 0x6c, 0x6d, 0x6e}) {
                const float number = value(i);
                std::uint32_t bits; std::memcpy(&bits, &number, sizeof(bits));
                events.script.push_back({1, false, {0, bits}, {0, 0}, {1, unsigned(i)}});
            }
            visual.enter(events, state, inputs, facing_right);
            state.variables[0x6b] = 0.0f;
            if (value(0x3a) != 0) ticks = 0;
            if (ticks || !alive) break;
        } while (++immediate < 64);
        if (ticks) --ticks;
        for (auto i : {0x80, 0x81, 0x82, 0x89, 0x8a}) {
            const int timer = int(value(i));
            state.variables[i] = float(timer > 0 ? timer - 1 : timer < 0 ? timer + 1 : 0);
        }
        x = value(0xa0); y = value(0xa1);
        facing_right = value(0x38) > 0;
        const auto mode = value(0x3b);
        if (mode != 4) {
            x += value(0x9e) * (facing_right ? 1 : -1);
            y += value(0x9f);
            if (mode == 1 || mode == 2) state.variables[0x9f] = value(0x9f) - value(0x36);
            // Landing links test the incoming downward velocity on the next
            // update. Clipping to the floor must not erase that evidence.
            if (floor_collision && (mode == 1 || mode == 2 || hit) && y <= 0 && value(0x9f) < 0) y = 0;
            if (mode == 1 || mode == 3) state.variables[0x9e] = std::copysign(
                std::max(0.0f, std::abs(value(0x9e)) - value(0x37)), value(0x9e));
        }
    }

    void advance(const FoitScriptState& parent, bool owner_active, bool frozen = false) {
        if (!alive || (frozen && !(unsigned(value(0x8f)) & 8u))) return;
        if (hit_stop) { --hit_stop; return; }
        if (authored_timeline) { advance_authored(parent); return; }
        const auto& script = table->scripts[move];
        const bool positioned_by_owner = std::any_of(script.begin(), script.end(), [](const auto& expression) {
            return expression.destination.flags == 1 &&
                (expression.destination.value == 0xa0 || expression.destination.value == 0xa1) &&
                (expression.left.flags == 9 || expression.left.flags == 0x19);
        });
        if (!persistent_owner && !independent_owner && !owner_active && (value(0x39) == 1 || positioned_by_owner)) {
            state.variables[0xb0] = 1.0f;
            const auto end = std::find(table->names.begin(), table->names.end(), "End");
            if (end == table->names.end()) {
                alive = false;
                return;
            }
            const auto target = unsigned(end - table->names.begin());
            if (move != target) start(target);
        }
        if (combat_entity) { advance_authored(parent); return; }
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
                auto next = table->successors[move];
                for (const auto& link : table->transitions[move]) {
                    if (link.target != move && matches(link.condition, parent)) { next = link.target; break; }
                }
                start(next);
                continue;
            }
            const auto& frame = clip[record++];
            state.variables[0x3a] = 0.0f;
            apply(frame.script, parent);
            descriptor(frame);
            ticks = frame.ticks;
            if (frame.sprites[1]) pose = frame.sprites[1];
            alive = !frame.destroys_entity;
            for (const auto& variant : frame.variants) {
                if (!matches(variant.condition, parent)) continue;
                apply(variant.script, parent);
                if (variant.clear_hit) hit = nullptr;
                if (variant.hit) { hit = &*variant.hit; hit_resolved = false; }
                if (variant.duration) ticks = *variant.duration;
                if (variant.destroys_entity) alive = false;
            }
            visual.enter(frame, state, FoitInputHistory{}, facing_right);
            if (value(0x3a) != 0) ticks = 0;
        }
        if (ticks) --ticks;
        state.variables[0x3a] = 0.0f;
        apply(table->scripts[move], parent);
        if (const auto& tick_override = table->tick_overrides[move]; tick_override) {
            const bool base_matches=matches(tick_override->condition,parent);
            const auto execute = [&](const auto& data) {
                FoitAnimationFrame events{};
                events.script=data.script; events.sounds=data.sounds;
                visual.enter(events,state,inputs,input_facing.value_or(facing_right));
                for (const auto& spawn : data.spawns)
                    visual.pending.push_back({0, &spawn, 0, 0, 1, false, state});
                apply(data.script,parent);
                if(data.clear_hit)hit=nullptr;
                if(data.hit){hit=&*data.hit;hit_resolved=false;}
                if(data.destroys_entity)alive=false;
            };
            if(base_matches)execute(tick_override->frame);
            for (const auto& variant : tick_override->frame.variants) {
                if(!base_matches && !persistent_owner)break;
                if (!matches(variant.condition, parent)) continue;
                execute(variant);
            }
        }
        x = value(0xa0);
        y = value(0xa1);
        const auto mode = value(0x3b);
        facing_right = value(0x38) > 0;
        if (mode != 4) {
            x += value(0x9e) * (facing_right ? 1.0f : -1.0f);
            y += value(0x9f);
            if (mode == 1 || mode == 2) state.variables[0x9f] = value(0x9f) - value(0x36);
            if (floor_collision && (mode == 1 || mode == 2) && y <= 0 && value(0x9f) < 0) {
                y = 0;
                if (!persistent_owner) state.variables[0x9f] = 0.0f;
            }
            if (mode == 1 || mode == 3) state.variables[0x9e] = std::copysign(
                std::max(0.0f, std::abs(value(0x9e)) - value(0x37)), value(0x9e));
        }
    }
};

}
