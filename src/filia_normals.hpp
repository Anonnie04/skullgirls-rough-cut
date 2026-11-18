#pragma once

#include <cstdint>

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
    { // Born With It (move 170)
        {0, 1}, {543, 6}, {0, 1}, {544, 6}, {0, 1}, {545, 6}, {546, 7},
        {547, 8}, {548, 5}, {0, 1}, {549, 1}, {0, 1}, {549, 5},
        {550, 6}, {551, 6}, {552, 6}, {0, 1}, {553, 7}, {0, 1}, {554, 8},
    },
    { // Tricobezoar (move 191)
        {0, 1}, {656, 4}, {657, 4}, {0, 1}, {658, 1}, {0, 1}, {658, 3}, {659, 3},
        {0, 1}, {660, 3}, {661, 4}, {662, 4}, {661, 3}, {663, 3}, {662, 3}, {661, 3},
        {663, 3}, {662, 3}, {664, 4}, {0, 1}, {0, 1}, {665, 2}, {0, 1}, {0, 1},
        {666, 2}, {0, 1}, {667, 4}, {668, 4}, {669, 3}, {670, 3}, {669, 3}, {670, 3},
        {669, 3}, {670, 3}, {669, 3}, {0, 1}, {670, 3}, {669, 3}, {671, 4}, {0, 1},
        {672, 2}, {673, 2}, {0, 1}, {674, 4}, {0, 1}, {675, 4}, {0, 1}, {676, 4},
        {0, 1}, {677, 4}, {678, 4}, {679, 4}, {680, 4},
    },
    { // Gregor start (move 171)
        {0, 1}, {555, 4}, {0, 1}, {556, 1}, {0, 1}, {556, 3}, {557, 7}, {0, 1},
        {0, 1}, {558, 4}, {559, 4}, {560, 4}, {561, 4}, {562, 4}, {0, 1}, {563, 10},
        {564, 4}, {565, 4}, {566, 7}, {567, 3}, {0, 1}, {568, 3}, {0, 1}, {569, 3},
        {0, 1}, {570, 3}, {0, 1}, {571, 3}, {0, 1}, {572, 3}, {0, 1}, {573, 3},
        {0, 1}, {574, 3}, {0, 1}, {575, 3}, {0, 1}, {576, 3}, {0, 1}, {577, 4},
        {0, 1}, {578, 3}, {0, 1}, {579, 3}, {0, 1}, {0, 1}, {580, 4}, {0, 1},
    },
    { // Gregor loop (move 172)
        {0, 1}, {581, 3}, {0, 1}, {582, 2}, {0, 1}, {583, 2}, {0, 1},
    },
    { // Gregor end (move 173)
        {0, 1}, {584, 3}, {0, 1}, {585, 2}, {0, 1}, {586, 3}, {0, 1}, {587, 2},
        {0, 1}, {588, 3}, {0, 1}, {589, 2}, {0, 1}, {590, 1}, {0, 1}, {0, 1},
        {591, 1}, {592, 2}, {593, 1}, {0, 1}, {594, 2}, {595, 1},
    },
    { // Gregor skid (move 174)
        {0, 1}, {596, 3}, {597, 3}, {598, 2}, {598, 1},
    },
    { // Gregor stop (move 175)
        {0, 1}, {599, 5}, {600, 5}, {601, 5},
    },
    { // Gregor air start (move 176)
        {0, 1}, {0, 1}, {568, 3}, {0, 1}, {0, 1}, {569, 1}, {0, 1}, {569, 4},
        {0, 1}, {0, 1}, {0, 1}, {570, 3}, {0, 1}, {570, 3}, {0, 1}, {571, 3},
        {0, 1}, {572, 3}, {0, 1}, {573, 3}, {0, 1}, {574, 3}, {0, 1}, {575, 3},
        {0, 1}, {576, 3}, {0, 1}, {577, 4}, {0, 1}, {578, 3}, {0, 1}, {579, 3},
        {0, 1}, {0, 1}, {580, 4}, {0, 1},
    },
    { // Gregor air loop (move 177)
        {0, 1}, {602, 3}, {603, 2}, {604, 2}, {0, 1},
    },
    { // Gregor air end (move 178)
        {0, 1}, {584, 3}, {0, 1}, {585, 2}, {585, 3}, {0, 1}, {586, 5}, {0, 1},
        {587, 5}, {0, 1}, {588, 5}, {0, 1},
    },
    { // Gregor fall (move 179)
        {171, 1}, {171, 2}, {172, 3}, {173, 2}, {174, 3},
    },
    { // Gregor fall loop (move 180)
        {175, 1}, {175, 30},
    },
    { // Gregor land (move 181)
        {0, 1}, {114, 2}, {115, 3}, {116, 4}, {0, 1},
    },
    { // Fenrir start (move 182)
        {0, 1}, {605, 3}, {0, 1}, {606, 1}, {0, 1}, {606, 2}, {607, 3}, {0, 1},
        {0, 1}, {608, 20}, {609, 3}, {0, 1}, {610, 3}, {611, 3}, {0, 1}, {612, 3},
        {613, 3}, {0, 1}, {614, 3}, {615, 3}, {616, 3}, {617, 3}, {0, 1}, {0, 1},
        {618, 1}, {618, 3},
    },
    { // Fenrir rise (move 183)
        {0, 1}, {619, 2}, {620, 2}, {621, 2}, {622, 2},
    },
    { // Fenrir peak (move 184)
        {0, 1}, {623, 4}, {624, 2}, {0, 1},
    },
    {}, // Fenrir whiff reuses the fall and landing records from light Updo.
    { // Fenrir peak hit (move 186)
        {0, 1}, {625, 4}, {626, 3}, {627, 4}, {628, 3}, {629, 4}, {0, 1}, {630, 3},
        {0, 1}, {631, 4}, {0, 1},
    },
    { // Fenrir bite wait (move 187)
        {0, 1}, {632, 3}, {633, 3},
    },
    { // Fenrir bite (move 188)
        {0, 1}, {634, 3}, {635, 2}, {635, 1}, {0, 1}, {636, 4}, {637, 15}, {0, 1},
        {637, 2}, {638, 2},
    },
    { // Fenrir dive (move 189)
        {0, 1}, {639, 3}, {640, 3},
    },
    { // Fenrir land (move 190)
        {0, 1}, {641, 1}, {641, 1}, {642, 2}, {643, 4}, {0, 1}, {643, 2}, {0, 1},
        {644, 3}, {645, 3}, {646, 3}, {647, 3}, {648, 3}, {0, 1}, {649, 5}, {650, 6},
        {651, 5}, {652, 3}, {653, 3}, {654, 3}, {655, 3},
    },
    { // Snapback (move 196)
        {0, 1}, {681, 4}, {682, 4}, {683, 3}, {683, 1}, {0, 1}, {684, 4}, {0, 1},
        {684, 30}, {0, 1}, {685, 3}, {0, 1}, {686, 3}, {0, 1}, {687, 3}, {0, 1},
        {688, 3}, {0, 1}, {689, 4}, {0, 1}, {690, 4}, {691, 5}, {692, 5}, {693, 5},
    },
};

constexpr int filia_launcher = 2;
constexpr int rapid_stand_lp = 18;
constexpr int rapid_crouch_lp = 19;

// FOIT move indices in Training's normal order, including the rapid repeats.
constexpr unsigned filia_foit_normals[] = {
    97, 100, 102, 99, 101, 103,
    104, 107, 109, 106, 108, 110,
    111, 113, 115, 112, 114, 116,
    98, 105,
};

constexpr int filia_normal_from_foit(int move) {
    for (int i = 0; i <= rapid_crouch_lp; ++i)
        if (move == static_cast<int>(filia_foit_normals[i])) return i;
    return -1;
}

// Bits refer to the original zero-based records. These normals assign 1
// to FOIT variable 0x3a, so FUN_00498ae0 advances again before rendering.
// Keeping the source indices also keeps the hit-descriptor indices valid.
constexpr unsigned next_visible_filia_normal_record(int move, unsigned record) {
    constexpr std::uint32_t immediate[] = {
        0xa215, 0x15, 0x54015, 0x46805, 0x5515, 0x55ab, // Standing
        0x5, 0x245, 0x115, 0x105, 0x1555545, 0xb, // Crouching
        0x15, 0x15551, 0x9, 0x15, 0xa951, 0x513, // Airborne
        0xa01, 0x5, // Rapid-fire jabs
    };
    if (move < 0 || move > rapid_crouch_lp) return record;
    while (record < 32 && (immediate[move] & (std::uint32_t{1} << record))) ++record;
    return record;
}

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
constexpr int filia_taunt = 27;
constexpr int tricobezoar = 28;
// Move 191 launches child 3 on record 21. Record 9 starts the sparkle emitter.
constexpr unsigned tricobezoar_spawn_frame = 20;
constexpr unsigned tricobezoar_charge_ticks = 2;
constexpr unsigned tricobezoar_start_ticks = 4;
constexpr int gregor_start = 29;
constexpr int gregor_loop = 30;
constexpr int gregor_end = 31;
constexpr int gregor_skid = 32;
constexpr int gregor_stop = 33;
constexpr int gregor_air_start = 34;
constexpr int gregor_air_loop = 35;
constexpr int gregor_air_end = 36;
constexpr int gregor_fall = 37;
constexpr int gregor_fall_loop = 38;
constexpr int gregor_land = 39;
constexpr int fenrir_start = 40;
constexpr int fenrir_rise = 41;
constexpr int fenrir_peak = 42;
constexpr int fenrir_fall = 43;
// Moves 185, 167 and 168 share this part of the light Updo sequence.
constexpr unsigned fenrir_fall_start = 19;
constexpr unsigned fenrir_fall_loop = 26;
constexpr unsigned fenrir_whiff_landing = 31;
constexpr int fenrir_peak_hit = 44;
constexpr int fenrir_bite_wait = 45;
constexpr int fenrir_bite = 46;
constexpr int fenrir_dive = 47;
constexpr int fenrir_land = 48;
constexpr int filia_snapback = 49;
// Quarter-circle enters 141/143/145 for two records. The half-circle
// routes through 140/142/144 for one additional record first.
constexpr unsigned ringlet_quarter_entry_ticks = 2;
constexpr unsigned ringlet_half_entry_ticks = 3;
constexpr unsigned ringlet_spawn_frame = 9;
constexpr float ringlet_distance[] = {170, 470, 770};
// Ringlet's child move at 0x1c1166 points into Filia's main sprite table.
// The descriptor is armed on record 6 and cleared by the flag on record 16.
constexpr unsigned ringlet_first_pose_frame = 1;
constexpr unsigned ringlet_spike_arm_frame = 6;
constexpr unsigned ringlet_spike_clear_frame = 16;
constexpr unsigned ringlet_short_spin_end = 24;
constexpr unsigned ringlet_dissolve_frame = 36;
constexpr unsigned ringlet_unlock_frame = 38;
constexpr NormalFrame ringlet_spike_frames[] = {
    {0, 1}, {1170, 4}, {0, 1}, {0, 1}, {0, 1}, {1171, 3}, {1172, 3},
    {1173, 2}, {1174, 2}, {1175, 2}, {1173, 2}, {1174, 2}, {1175, 2},
    {1173, 2}, {1174, 2}, {1175, 2}, {0, 1},
    {1173, 2}, {1174, 2}, {1175, 2}, {1173, 2}, {1174, 2}, {1175, 2},
    {1173, 2}, {1174, 2}, {1175, 2}, {1173, 2}, {1174, 2}, {1175, 2},
    {1173, 2}, {1174, 2}, {1175, 2}, {1173, 2}, {1174, 2}, {1175, 2}, {1173, 2},
    {1176, 3}, {1177, 3}, {1178, 3}, {1179, 3}, {1179, 1},
};
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
