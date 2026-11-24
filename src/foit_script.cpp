#include "foit_script.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace skullgirls {
namespace {

std::optional<float> literal(std::uint32_t bits)
{
    float value;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return std::isfinite(value) ? std::optional<float>{value} : std::nullopt;
}

std::optional<std::int32_t> script_integer(float value)
{
    // Use double for the range check because INT32_MAX rounds up as a float.
    const auto truncated = std::trunc(static_cast<double>(value));
    if (truncated >= std::numeric_limits<std::int32_t>::min() &&
        truncated <= std::numeric_limits<std::int32_t>::max()) {
        return static_cast<std::int32_t>(truncated);
    }
    return std::nullopt;
}

std::optional<std::size_t> bit_index(FoitOperand operand)
{
    const auto value = literal(operand.value);
    if (value && *value >= 0 && *value < 128 && std::trunc(*value) == *value)
        return static_cast<std::size_t>(*value);
    return std::nullopt;
}

std::optional<float> read_operand(FoitOperand operand, const FoitScriptState& state)
{
    switch (operand.flags) {
    case 0: case 0x24:
        return literal(operand.value);
    case 1:
        if (operand.value < state.variables.size()) return state.variables[operand.value];
        break;
    case 0x19:
        if (operand.value < state.team_variables.size()) return state.team_variables[operand.value];
        break;
    case 0x20:
        if (const auto index = bit_index(operand); index && state.bits[*index])
            return *state.bits[*index] ? 1.0f : 0.0f;
        break;
    }
    return std::nullopt;
}

// FUN_0048ce00 compares operands A and C. Operand B belongs to arithmetic.
std::optional<bool> compare(const FoitExpression& expression, const FoitScriptState& state)
{
    if (!expression.comparison) return std::nullopt;
    const auto left = read_operand(expression.left, state);
    const auto right = read_operand(expression.destination, state);
    if (!left || !right) return std::nullopt;
    switch (expression.opcode) {
    case 0x10: return *left < *right;
    case 0x11: return *left > *right;
    case 0x12: case 0x16: return *left == *right;
    case 0x13: return *left >= *right;
    case 0x14: return *left <= *right;
    case 0x15: case 0x17: return *left != *right;
    case 0x18: return true;
    default: return std::nullopt;
    }
}

bool button_matches(unsigned expected, unsigned actual)
{
    if (expected == actual || expected == 10 || actual == 10) return true;
    if (expected == 2) return actual <= 1;
    if (actual == 2) return expected <= 1;
    switch (expected) {
    case 5: return actual != 0;
    case 6: return actual != 1;
    case 7: return actual >= 3;
    case 8: return actual != 3;
    case 9: return actual != 4;
    default: return false;
    }
}

std::optional<bool> matches_step(const FoitInputStep& step, const FoitInputSample& input,
                                 bool facing_right, const FoitScriptState& state)
{
    constexpr unsigned mirrored[] = {4,3,2,1,0,7,6,5,8,9,10};
    if (input.direction >= std::size(mirrored) || input.direction == 10 || step.directions[0] == 10)
        return false;
    const auto direction = facing_right ? input.direction : mirrored[input.direction];
    if (step.directions[0] != 9 && direction != 9) {
        const auto matches = std::find(step.directions.begin(), step.directions.end(), direction) != step.directions.end();
        if (matches == step.invert_direction) return false;
    }
    for (std::size_t i = 0; i < step.buttons.size(); ++i)
        if (!button_matches(step.buttons[i], input.buttons[i])) return false;
    for (const auto& test : step.tests) {
        const auto result = compare(test, state);
        if (!result || !*result) return result;
    }
    return true;
}

bool matches_command(const FoitCondition& condition, const FoitInputHistory& history, unsigned start,
                     bool facing_right, const FoitScriptState& state)
{
    const auto matches = [&](const FoitInputStep& step, unsigned age) {
        return matches_step(step, history[age], facing_right, state).value_or(false);
    };
    const auto rejected = [&](unsigned age) {
        return condition.reject && matches_step(*condition.reject, history[age], facing_right, state).value_or(true);
    };
    const auto& steps = condition.steps;
    if (!matches(steps.front(), start)) return false;
    if (condition.window != 1 && rejected(start)) return false;
    if (steps.size() == 1) return true;

    // FUN_0046dd60 walks from the trigger toward older samples. Holding a
    // previous step across multiple samples leaves the next step eligible.
    const auto end = std::min<unsigned>(start + condition.window, static_cast<unsigned>(history.size()));
    std::size_t step = 1;
    bool matched_older_step = false;
    for (unsigned age = start + 1; age < end; ++age) {
        if (rejected(age)) return false;
        const auto& wanted = steps[step];
        bool match = wanted.hold < end - age && matches(wanted, age);
        for (unsigned held = 1; match && held <= wanted.hold; ++held)
            match = matches(wanted, age + held);
        if (match) {
            if (++step == steps.size()) return true;
            matched_older_step = true;
        } else if (matches(steps[step - 1], age)) {
            if (!matched_older_step) return false;
        } else {
            matched_older_step = false;
        }
    }
    return false;
}

}

FoitInputSample foit_input_sample(unsigned direction, unsigned pressed, unsigned held, unsigned released)
{
    FoitInputSample sample;
    sample.direction = static_cast<std::uint8_t>(direction);
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned bit = 1u << i;
        sample.buttons[i] = pressed & bit ? 0 : held & bit ? 3 : released & bit ? 1 : 4;
    }
    // FUN_0046c440/0046c5b0 synthesize the Any/Two/Three Punch and Kick states.
    for (unsigned group = 0; group < 2; ++group) {
        const auto first = sample.buttons.begin() + group * 3;
        const auto fresh = std::count(first, first + 3, 0);
        const auto down = std::count(first, first + 3, 3);
        const auto up = std::count(first, first + 3, 1);
        for (unsigned required = 1; required <= 3; ++required) {
            sample.buttons[6 + (required - 1) * 2 + (1 - group)] =
                fresh >= required ? 0 : up >= required ? 1 : down >= required ? 3 : 4;
        }
    }
    return sample;
}

// Find the newest buffered sample that satisfies the command. Age zero is the newest sample.
std::optional<unsigned> match_foit_condition(const FoitCondition& condition, const FoitInputHistory& history,
                                             bool facing_right, const FoitScriptState& state,
                                             unsigned oldest_trigger)
{
    if (condition.steps.empty()) return std::nullopt;
    const auto last = std::min({unsigned(condition.buffer), oldest_trigger, unsigned(history.size() - 1)});
    for (unsigned age = 0; age <= last; ++age)
        if (matches_command(condition, history, age, facing_right, state)) return age;
    return std::nullopt;
}

bool test_foit_expressions(const std::vector<FoitExpression>& expressions, const FoitScriptState& state)
{
    for (const auto& expression : expressions) {
        const auto result = compare(expression, state);
        if (!result || !*result) return false;
    }
    return true;
}

void apply_foit_assignment(const FoitExpression& expression, FoitScriptState& state)
{
    if (expression.comparison) return;
    const auto destination = expression.destination;
    // Only emulate writable state that the caller explicitly enabled.
    std::optional<float>* variable = nullptr;
    std::optional<bool>* bit = nullptr;
    if (destination.flags == 1 && destination.value < state.variables.size() && state.variables[destination.value])
        variable = &state.variables[destination.value];
    else if (destination.flags == 0x20) {
        if (const auto index = bit_index(destination); index && state.bits[*index]) bit = &state.bits[*index];
    }
    if (!variable && !bit) return;

    const auto left = read_operand(expression.left, state);
    const auto right = read_operand(expression.right, state);
    if (!left || !right) return;
    float result;
    switch (expression.opcode) {
    case 1: result = *left; break;
    case 2: result = *left + *right; break;
    case 3: result = *left - *right; break;
    case 4: result = *left * *right; break;
    case 5: if (*right == 0) return; result = *left / *right; break;
    case 6: result = std::abs(*left); break;
    case 7: result = std::trunc(*left); break;
    // FUN_0048c260's trigonometric operands are degrees.
    case 9: result = float(std::sin(double(*left) * 0.017453292519943295)); break;
    case 10: result = float(std::cos(double(*left) * 0.017453292519943295)); break;
    case 0x0b: result = std::max(*left, *right); break;
    case 0x0c: result = std::min(*left, *right); break;
    case 0x0e: if (*right == 0) return; result = *left; break;
    case 0x0f: if (*right != 0) return; result = *left; break;
    case 0x1c: case 0x1d: case 0x1e: {
        const auto a = script_integer(*left);
        const auto b = script_integer(*right);
        if (!a || !b) return;
        if (expression.opcode == 0x1c) {
            result = static_cast<float>(*a | *b);
        } else if (expression.opcode == 0x1d) {
            result = static_cast<float>(*a & *b);
        } else {
            if (*b == 0) return;
            // Widen the division so INT32_MIN % -1 is defined.
            result = static_cast<float>(static_cast<std::int64_t>(*a) % *b);
        }
        break;
    }
    case 0x23: result = *left == *right ? 1.0f : 0.0f; break;
    case 0x24: result = *left != *right ? 1.0f : 0.0f; break;
    case 0x25: result = *left < *right ? 1.0f : 0.0f; break;
    default: return;
    }
    if (variable) *variable = result;
    else *bit = result != 0;
}

void apply_foit_script(const std::vector<FoitExpression>& expressions, FoitScriptState& state,
                       const FoitScriptState& defaults)
{
    // FUN_004918c0 executes each arithmetic node. Comparisons belong to the
    // separate condition evaluator and are ignored by FUN_0048c260.
    for (const auto& expression : expressions) {
        if (expression.comparison) continue;
        if (expression.opcode == 8 && expression.left.flags == 1 && expression.destination.flags == 1 &&
            expression.left.value < defaults.variables.size() && expression.destination.value < state.variables.size() &&
            defaults.variables[expression.left.value] && state.variables[expression.destination.value]) {
            state.variables[expression.destination.value] = defaults.variables[expression.left.value];
        } else {
            apply_foit_assignment(expression, state);
        }
    }
}

}
