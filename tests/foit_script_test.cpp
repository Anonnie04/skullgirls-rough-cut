#include "foit_script.hpp"

#include <iostream>
#include <cmath>
#include <stdexcept>

namespace {

void check(bool result, const char* message)
{
    if (!result) throw std::runtime_error(message);
}

skullgirls::FoitInputStep step(std::uint8_t direction)
{
    skullgirls::FoitInputStep result{};
    result.directions = {direction, 10, 10};
    result.buttons.fill(10);
    return result;
}

}

int main()
{
    using namespace skullgirls;
    try {
        FoitScriptState state;
        FoitInputHistory inputs{};
        auto punch = step(9);
        punch.buttons[1] = 0;
        punch.buttons[6] = 5;
        // The right comparison operand is C, even when B is present.
        punch.tests.push_back({0x10, true, {1, 0x7f}, {0, 0}, {0, 0x40400000}});
        const FoitCondition buffered{1, 5, 0, {punch}, {}};
        inputs[5] = foit_input_sample(8, 2, 2, 0);
        check(!match_foit_condition(buffered, inputs, true, state), "Unknown variables gate the rule");
        state.variables[0x7f] = 1.0f;
        check(match_foit_condition(buffered, inputs, true, state) == 5, "The last buffered sample matches");
        check(!match_foit_condition(buffered, inputs, true, state, 4), "The initiating edge is outside the new move");
        inputs[5] = foit_input_sample(8, 2 | 8, 2 | 8, 0);
        check(!match_foit_condition(buffered, inputs, true, state), "A simultaneous kick suppresses the punch rule");

        auto forward_punch = step(0);
        forward_punch.buttons[0] = 2;
        auto down = step(2);
        down.hold = 1;
        const FoitCondition motion{5, 0, 2, {forward_punch, step(1), down}, step(4)};
        inputs = {};
        inputs[0] = foit_input_sample(4, 1, 1, 0);
        inputs[1].direction = 3;
        inputs[2].direction = inputs[3].direction = 2;
        check(match_foit_condition(motion, inputs, false, state) == 0, "The mirrored motion and held step match");
        check(!match_foit_condition(motion, inputs, true, state), "Facing changes the matched directions");
        inputs[3].direction = 0;
        check(!match_foit_condition(motion, inputs, false, state), "A missing hold fails the motion");
        inputs[1].direction = 0;
        inputs[2].direction = 3;
        inputs[3].direction = inputs[4].direction = 2;
        check(!match_foit_condition(motion, inputs, false, state), "The reject pattern interrupts an otherwise valid motion");

        const auto held_punch = foit_input_sample(8, 0, 3, 0);
        const auto released_punch = foit_input_sample(8, 0, 0, 3);
        check(held_punch.buttons[0] == 3 && held_punch.buttons[9] == 3,
            "Native state 3 means held, including synthesized two-punch input");
        check(released_punch.buttons[0] == 1 && released_punch.buttons[9] == 1,
            "Native state 1 means released, including synthesized two-punch input");
        auto negative_edge = step(9); negative_edge.buttons[0] = 2;
        inputs = {}; inputs[0] = released_punch;
        check(match_foit_condition({1, 0, 0, {negative_edge}, {}}, inputs, true, state) == 0,
            "State 2 accepts a release edge");
        inputs[0] = held_punch;
        check(!match_foit_condition({1, 0, 0, {negative_edge}, {}}, inputs, true, state),
            "Holding a button does not create another command edge");
        const auto mixed = foit_input_sample(8, 1, 3, 0);
        check(mixed.buttons[7] == 0 && mixed.buttons[9] == 4, "Two-punch edge requires two fresh presses");
        const auto two_punches = foit_input_sample(8, 3, 3, 0);
        check(two_punches.buttons[9] == 0, "Two fresh punches set the two-punch slot");

        state.bits[0] = false;
        apply_foit_assignment({1, false, {0, 0x3f800000}, {0, 0}, {0x20, 0}}, state);
        check(state.bits[0] == true, "The frame script can open the rapid-fire flag");
        apply_foit_assignment({3, false, {1, 0x7f}, {0, 0x3f800000}, {1, 0x7f}}, state);
        check(state.variables[0x7f] == 0.0f, "The frame script updates an enabled variable");

        state.variables[0xa8] = 90.0f; state.variables[0x9e] = state.variables[0x9f] = 0.0f;
        apply_foit_assignment({9, false, {1, 0xa8}, {0, 0}, {1, 0x9f}}, state);
        apply_foit_assignment({10, false, {1, 0xa8}, {0, 0}, {1, 0x9e}}, state);
        check(std::abs(*state.variables[0x9f] - 1.0f) < 0.00001f &&
              std::abs(*state.variables[0x9e]) < 0.00001f,
              "Authored projectile angles use sine/cosine in degrees");
        auto defaults = state;
        defaults.variables[0x7f] = 3.0f;
        apply_foit_script({{0x10, true, {0, 0x3f800000}, {0, 0}, {0, 0}},
                          {8, false, {1, 0x7f}, {0, 0}, {1, 0x7f}}}, state, defaults);
        check(state.variables[0x7f] == 3.0f, "Default-slot assignment executes in the arithmetic list");
        auto team_check = step(9);
        team_check.tests.push_back({0x13, true, {0x19, 0x42}, {0, 0}, {0, 0x42c80000}});
        inputs = {};
        inputs[0] = foit_input_sample(8, 0, 0, 0);
        const FoitCondition team_meter{1, 0, 0, {team_check}, {}};
        check(!match_foit_condition(team_meter, inputs, true, state), "Unknown team resources gate a command");
        state.team_variables[0x42] = 100.0f;
        check(match_foit_condition(team_meter, inputs, true, state) == 0, "Team meter can enable Snapback");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
