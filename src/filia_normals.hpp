#pragma once

namespace skullgirls {

struct NormalFrame { unsigned sprite, ticks; };

// LP, MP, HP, LK, MK, HK for standing, crouching, and aerial normals.
constexpr NormalFrame filia_normals[][64] = {
    { // FOIT move 97
        {0, 1}, {198, 1}, {0, 1}, {198, 1}, {0, 1}, {198, 1}, {199, 2}, {200, 1},
        {200, 2}, {0, 1}, {200, 3}, {201, 3}, {202, 4}, {0, 1}, {203, 4}, {0, 1},
    },
    { // FOIT move 100
        {0, 1}, {220, 1}, {0, 1}, {220, 2}, {0, 1}, {221, 2}, {222, 3}, {223, 3},
        {224, 2}, {225, 3}, {226, 3}, {227, 4}, {228, 4}, {229, 3}, {230, 3}, {231, 2},
    },
    { // FOIT move 102
        {0, 1}, {240, 1}, {0, 1}, {240, 2}, {0, 1}, {241, 3}, {242, 4}, {243, 2},
        {244, 1}, {245, 2}, {246, 3}, {247, 3}, {248, 3}, {249, 3}, {0, 1}, {250, 3},
        {0, 1}, {251, 3}, {0, 1}, {252, 3},
    },
    { // FOIT move 99
        {0, 1}, {208, 2}, {0, 1}, {209, 3}, {210, 2}, {211, 1}, {212, 1}, {213, 1},
        {213, 2}, {214, 3}, {215, 4}, {216, 1}, {216, 3}, {217, 1}, {0, 1}, {217, 3},
        {218, 3}, {219, 3}, {219, 1},
    },
    { // FOIT move 101
        {0, 1}, {232, 4}, {0, 1}, {233, 3}, {0, 1}, {234, 3}, {235, 3}, {235, 3},
        {0, 1}, {236, 3}, {0, 1}, {237, 4}, {0, 1}, {238, 3}, {0, 1}, {239, 4},
    },
    { // FOIT move 103
        {0, 1}, {0, 1}, {253, 2}, {0, 1}, {254, 3}, {0, 1}, {255, 3}, {0, 1},
        {0, 1}, {256, 3}, {0, 1}, {257, 7}, {0, 1}, {257, 1}, {0, 1}, {258, 2},
        {259, 7}, {259, 3}, {260, 4}, {261, 6}, {262, 5}, {263, 5}, {264, 3},
    },
    { // FOIT move 104
        {0, 1}, {265, 1}, {0, 1}, {265, 3}, {266, 2}, {267, 3}, {268, 3}, {269, 4},
    },
    { // FOIT move 107
        {0, 1}, {275, 1}, {0, 1}, {275, 1}, {276, 2}, {277, 5}, {0, 1}, {278, 2},
        {279, 2}, {0, 1}, {280, 3}, {281, 3}, {282, 4}, {283, 4}, {284, 6}, {285, 6},
        {286, 6}, {287, 3}, {288, 3}, {289, 3},
    },
    { // FOIT move 109
        {0, 1}, {308, 1}, {0, 1}, {308, 2}, {0, 1}, {309, 3}, {310, 3}, {311, 3},
        {0, 1}, {312, 2}, {313, 1}, {314, 2}, {315, 3}, {316, 3}, {317, 4}, {318, 4},
        {319, 4}, {320, 3}, {321, 3}, {322, 3}, {323, 3},
    },
    { // FOIT move 106
        {0, 1}, {270, 5}, {0, 1}, {271, 2}, {272, 3}, {272, 4}, {273, 3}, {274, 3},
        {274, 1},
    },
    { // FOIT move 108
        {0, 1}, {290, 3}, {0, 1}, {291, 3}, {292, 2}, {293, 2}, {0, 1}, {294, 2},
        {0, 1}, {295, 2}, {0, 1}, {296, 2}, {0, 1}, {297, 2}, {0, 1}, {298, 2},
        {0, 1}, {299, 2}, {0, 1}, {300, 2}, {0, 1}, {298, 2}, {0, 1}, {299, 2},
        {0, 1}, {300, 3}, {301, 3}, {302, 3}, {303, 3}, {304, 3}, {305, 3}, {306, 3},
        {307, 3},
    },
    { // FOIT move 110
        {0, 1}, {0, 1}, {324, 2}, {0, 1}, {325, 3}, {326, 2}, {327, 3}, {328, 2},
        {329, 2}, {330, 3}, {331, 3}, {332, 3}, {333, 3}, {330, 3}, {331, 3}, {332, 3},
        {333, 3}, {334, 6}, {335, 5}, {336, 4}, {337, 4},
    },
    { // FOIT move 111
        {0, 1}, {338, 2}, {0, 1}, {339, 3}, {0, 1}, {340, 2}, {341, 1}, {341, 3},
        {342, 2}, {343, 2}, {344, 2}, {345, 1},
    },
    { // FOIT move 113
        {0, 1}, {351, 2}, {352, 3}, {353, 3}, {0, 1}, {354, 3}, {0, 1}, {355, 2},
        {0, 1}, {356, 2}, {0, 1}, {355, 2}, {0, 1}, {356, 2}, {0, 1}, {355, 2},
        {0, 1}, {356, 2}, {357, 3}, {358, 2}, {359, 2}, {360, 2},
    },
    { // FOIT move 115
        {0, 1}, {376, 2}, {377, 2}, {0, 1}, {378, 4}, {379, 2}, {380, 2}, {381, 2},
        {382, 2}, {383, 2}, {384, 3}, {385, 3}, {386, 3}, {387, 3}, {388, 3},
    },
    { // FOIT move 112
        {0, 1}, {346, 2}, {0, 1}, {346, 2}, {0, 1}, {347, 3}, {348, 4}, {349, 4},
        {350, 4},
    },
    { // FOIT move 114
        {0, 1}, {361, 3}, {362, 3}, {363, 3}, {0, 1}, {364, 2}, {0, 1}, {365, 3},
        {0, 1}, {366, 3}, {367, 3}, {0, 1}, {368, 3}, {0, 1}, {369, 3}, {0, 1},
        {370, 3}, {371, 3}, {372, 3}, {373, 4}, {374, 3}, {375, 3},
    },
    { // FOIT move 116
        {0, 1}, {0, 1}, {389, 3}, {390, 3}, {0, 1}, {391, 3}, {392, 2}, {393, 2},
        {0, 1}, {394, 2}, {0, 1}, {394, 8}, {395, 5}, {396, 5}, {397, 4}, {398, 4},
    },
    { // FOIT move 98
        {0, 1}, {204, 2}, {205, 3}, {206, 1}, {206, 2}, {206, 3},
        {207, 4}, {201, 3}, {202, 3}, {0, 1}, {203, 3}, {0, 1},
    },
    { // FOIT move 105
        {0, 1}, {265, 1}, {0, 1}, {265, 3}, {266, 2}, {267, 3}, {268, 3}, {269, 4},
    },
    { // FOIT Updo 161, flight and landing 164-168
        {0, 1}, {510, 3}, {511, 5}, {0, 1}, {512, 2}, {0, 1}, {0, 1},
        {0, 1}, {0, 1}, {513, 2}, {0, 1}, {515, 2}, {516, 2}, {517, 2},
        {518, 2}, {0, 1}, {519, 4}, {520, 2}, {521, 4}, {0, 1}, {522, 2},
        {523, 2}, {524, 2}, {525, 2}, {0, 1}, {526, 3}, {0, 1}, {527, 2},
        {528, 2}, {529, 2}, {530, 2}, {0, 1}, {0, 1}, {531, 5}, {532, 2},
        {0, 1}, {533, 4}, {534, 4}, {535, 4}, {0, 1}, {536, 4}, {537, 4},
        {538, 3}, {539, 3}, {540, 3}, {541, 2}, {542, 2},
    },
    { // FOIT Updo 162, flight and landing 164-168
        {0, 1}, {510, 3}, {511, 7}, {0, 1}, {514, 2}, {0, 1}, {0, 1},
        {0, 1}, {0, 1}, {513, 2}, {0, 1}, {515, 2}, {516, 2}, {517, 2},
        {518, 2}, {0, 1}, {519, 4}, {520, 2}, {521, 4}, {0, 1}, {522, 2},
        {523, 2}, {524, 2}, {525, 2}, {0, 1}, {526, 3}, {0, 1}, {527, 2},
        {528, 2}, {529, 2}, {530, 2}, {0, 1}, {0, 1}, {531, 5}, {532, 2},
        {0, 1}, {533, 4}, {534, 4}, {535, 4}, {0, 1}, {536, 4}, {537, 4},
        {538, 3}, {539, 3}, {540, 3}, {541, 2}, {542, 2},
    },
    { // FOIT Updo 163, flight and landing 164-168
        {0, 1}, {507, 2}, {508, 2}, {510, 3}, {511, 6}, {0, 1}, {511, 1},
        {0, 1}, {512, 1}, {0, 1}, {0, 1}, {0, 1}, {0, 1}, {513, 2},
        {0, 1}, {515, 2}, {516, 2}, {517, 2}, {518, 2}, {0, 1}, {519, 4},
        {520, 2}, {521, 4}, {0, 1}, {522, 2}, {523, 2}, {524, 2}, {525, 2},
        {0, 1}, {526, 3}, {0, 1}, {527, 2}, {528, 2}, {529, 2}, {530, 2},
        {0, 1}, {0, 1}, {531, 5}, {532, 2}, {0, 1}, {533, 4}, {534, 4},
        {535, 4}, {0, 1}, {536, 4}, {537, 4}, {538, 3}, {539, 3}, {540, 3},
        {541, 2}, {542, 2},
    },
    { // Ground Hairball (moves 148-150, 154, 156)
        {0, 1}, {487, 4}, {488, 3}, {489, 4}, {0, 1}, {490, 3}, {0, 1},
        {491, 2}, {492, 2}, {493, 2}, {494, 2}, {0, 1}, {491, 3}, {495, 3},
        {0, 1}, {496, 2}, {497, 3}, {498, 2}, {499, 3}, {0, 1}, {500, 3},
        {501, 4}, {502, 5}, {503, 4},
    },
    { // Air Hairball (moves 151-153, 155, 157)
        {0, 1}, {488, 4}, {489, 5}, {490, 4}, {0, 1}, {0, 1}, {491, 2},
        {492, 2}, {493, 2}, {494, 2}, {0, 1}, {491, 3}, {504, 7}, {0, 1},
        {505, 5}, {506, 4},
    },
    { // Ringlet Psych (move 147)
        {0, 1}, {467, 2}, {0, 1}, {467, 3}, {468, 5}, {469, 4}, {0, 1},
        {471, 3}, {482, 3}, {483, 3}, {484, 3}, {485, 2},
    },
    { // Ringlet Spike (move 146)
        {0, 1}, {466, 2}, {0, 1}, {466, 1}, {467, 3}, {468, 3}, {469, 3},
        {470, 3}, {471, 3}, {0, 1}, {472, 3}, {473, 3}, {474, 3}, {475, 3},
        {476, 3}, {477, 3}, {478, 3}, {479, 3}, {0, 1}, {480, 3}, {481, 3},
        {482, 3}, {483, 3}, {484, 3}, {485, 3}, {486, 3},
    },
};

constexpr int rapid_stand_lp = 18;
constexpr int rapid_crouch_lp = 19;
constexpr int updo_light = 20;
constexpr int ground_hairball = 23;
struct UpdoMotion {
    unsigned lift_frame, flight_frame, flight_ticks;
    float forward, upward, gravity, friction;
};
// Initial motion writes in 161-163, followed by the timer in 164.
constexpr UpdoMotion updo_motion[] = {
    {6, 10, 4, 20, 30, 2.5f, 2.5f},
    {6, 10, 6, 21, 34, 2.5f, 2},
    {10, 14, 10, 24, 42, 2, 2.25f},
};
constexpr unsigned updo_air_loop_end = 20;
constexpr unsigned updo_air_loop_start = 15;
constexpr int air_hairball = 24;
constexpr int ringlet_psych = 25;
constexpr int ringlet_spike = 26;
// Quarter-circle enters 141/143/145 for two records. The half-circle
// routes through 140/142/144 for one additional record first.
constexpr unsigned ringlet_quarter_entry_ticks = 2;
constexpr unsigned ringlet_half_entry_ticks = 3;
constexpr unsigned ringlet_spawn_frame = 9;
constexpr float ringlet_distance[] = {170, 470, 770};
// Ringlet's child move at 0x1c1166 points into Filia's main sprite table.
// The descriptor is armed on record 6 and cleared by the flag on record 16.
// Shorten the cosmetic spin after record 16 so the dissolve ends with
// Filia's recovery. The visible startup and hit records keep their FOIT timing.
constexpr unsigned ringlet_spike_arm_frame = 6;
constexpr unsigned ringlet_spike_clear_frame = 16;
constexpr NormalFrame ringlet_spike_frames[] = {
    {0, 1}, {1170, 4}, {0, 1}, {0, 1}, {0, 1}, {1171, 3}, {1172, 3},
    {1173, 2}, {1174, 2}, {1175, 2}, {1173, 2}, {1174, 2}, {1175, 2},
    {1173, 2}, {1174, 2}, {1175, 2}, {0, 1},
    {1175, 1},
    {1176, 3}, {1177, 3}, {1178, 3}, {1179, 3}, {1179, 1},
};
static_assert([] {
    unsigned recovery = 0;
    for (unsigned i = ringlet_spawn_frame; filia_normals[ringlet_spike][i].ticks; ++i)
        recovery += filia_normals[ringlet_spike][i].ticks;
    unsigned drill = 0;
    for (const auto& frame : ringlet_spike_frames) drill += frame.ticks;
    return recovery == drill;
}(), "Ringlet Spike must disappear when the caster returns to idle");
// Startup and loop each have the same record counts for ground and air.
constexpr unsigned hairball_loop_frame = 6;
constexpr unsigned hairball_end_frame = 11;

// Input strength operands in the FOIT transition conditions, in keyboard order.
constexpr unsigned filia_normal_input_order[] = {1, 3, 5, 2, 4, 6};
// Each move initializes FOIT variable 0x7f. Standing HP and crouching HP write 6.
constexpr unsigned filia_normal_chain_order[] = {
    1, 3, 6, 2, 4, 6,
    1, 3, 6, 2, 4, 6,
    1, 3, 5, 2, 4, 6,
    1, 1,
};

constexpr int filia_normal_followup(int current, int button, bool airborne, bool down) {
    if (current >= updo_light) return -1;
    const int target = button + (airborne ? 12 : down ? 6 : 0);
    const bool current_air = current >= 12 && current < 18;
    if (current_air != airborne) return -1;
    return filia_normal_input_order[button] > filia_normal_chain_order[current] ? target : -1;
}

// Moves 97 and 104 open their rapid-fire whiff branch with FOIT bit 0.
constexpr bool filia_rapid_jab_whiff(int move, unsigned frame) {
    return (move == 0 && frame >= 11 && frame < 16) || (move == 6 && frame >= 6);
}

}
