#pragma once

#include "foit.hpp"

namespace skullgirls {

struct FoitInputSample {
    std::uint8_t direction = 8;
    std::array<std::uint8_t, 15> buttons = {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4};
};

using FoitInputHistory = std::array<FoitInputSample, 64>;

// A missing value means that the host has not implemented that engine variable.
struct FoitScriptState {
    std::vector<FoitNamedPoint> points;
    // FUN_0044d080 subtracts the layer offset before FUN_004971f0 adds
    // layer-offset minus origin: the combined point offset is -origin.
    std::array<std::int32_t,2> point_offset{};
    std::optional<std::vector<std::uint32_t>> move_categories;
    std::array<std::optional<float>, 0xb1> variables{};
    std::array<std::optional<bool>, 128> bits{};
    std::array<std::optional<float>, 0xb1> team_variables{};
};

FoitInputSample foit_input_sample(unsigned direction, unsigned pressed, unsigned held, unsigned released);
std::optional<unsigned> match_foit_condition(const FoitCondition& condition, const FoitInputHistory& history,
                                             bool facing_right, const FoitScriptState& state,
                                             unsigned oldest_trigger = FoitInputHistory{}.size() - 1);
void apply_foit_assignment(const FoitExpression& expression, FoitScriptState& state);
bool test_foit_expressions(const std::vector<FoitExpression>& expressions, const FoitScriptState& state);
void apply_foit_script(const std::vector<FoitExpression>& expressions, FoitScriptState& state,
                       const FoitScriptState& defaults);

}
