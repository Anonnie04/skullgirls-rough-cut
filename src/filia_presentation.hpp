#pragma once

#include "filia_defense.hpp"

namespace skullgirls {

constexpr NormalFrame result_turn_crouched[] = { // FOIT move 494
    {0, 1}, {54, 3}, {55, 3}, {56, 3}, {57, 1},
};

constexpr NormalFrame result_stand_up[] = { // FOIT move 495
    {45, 3}, {46, 3}, {47, 4}, {48, 4}, {49, 4},
};

constexpr NormalFrame result_turn[] = { // FOIT move 497
    {0, 1}, {50, 4}, {51, 4}, {52, 3}, {53, 3},
};

constexpr NormalFrame win_hair_blown[] = { // FOIT move 501
    {1036, 3}, {1037, 3}, {1038, 3}, {0, 1}, {1039, 3}, {1040, 3}, {1041, 3}, {1042, 3},
    {1043, 3}, {1044, 3}, {0, 1},
};

constexpr NormalFrame win_hair_blown_loop[] = { // FOIT move 502
    {1045, 3}, {1046, 3}, {1047, 3}, {1048, 3}, {1049, 3}, {1050, 3}, {1051, 3}, {1052, 3},
    {1053, 3}, {1054, 3}, {1055, 3}, {1056, 3}, {0, 1},
};

constexpr NormalFrame win_sassy[] = { // FOIT move 505
    {0, 1}, {0, 1}, {1057, 4}, {1058, 4}, {1059, 4}, {1060, 4}, {1061, 4}, {1062, 4},
    {1063, 4}, {1064, 4}, {1065, 4}, {1066, 4}, {1067, 4}, {1068, 4}, {1069, 4}, {1070, 4},
    {1071, 4}, {1072, 4}, {1073, 4}, {1074, 4}, {1075, 25},
};

constexpr NormalFrame win_sassy_loop[] = { // FOIT move 507
    {1075, 400},
};

constexpr NormalFrame win_filia_blown[] = { // FOIT move 508
    {0, 1}, {1076, 4}, {1077, 3}, {1078, 4}, {1079, 3}, {1080, 4}, {1081, 3}, {1082, 4},
    {1083, 3}, {1084, 4}, {1085, 3}, {1086, 4}, {0, 1},
};

constexpr NormalFrame win_filia_blown_loop[] = { // FOIT move 509
    {1087, 5}, {1088, 5}, {1089, 5}, {1090, 5}, {1091, 5}, {1092, 5}, {1093, 5}, {1094, 5},
    {0, 1},
};

constexpr NormalFrame lose_pull_hair[] = { // FOIT move 512
    {1095, 4}, {1096, 4}, {1097, 4}, {1098, 4}, {1099, 4}, {1100, 4}, {1101, 4}, {1102, 4},
    {1103, 4}, {1104, 4}, {1105, 4}, {0, 1},
};

constexpr NormalFrame lose_pull_hair_loop[] = { // FOIT move 513
    {1106, 4}, {1107, 4}, {1108, 4}, {1109, 4}, {1106, 4}, {1107, 4}, {1108, 4}, {1109, 4},
    {0, 1},
};

constexpr NormalFrame intro_hat[] = { // FOIT move 524
    {1110, 6}, {0, 1}, {1110, 2}, {1111, 2}, {1112, 4}, {1113, 6}, {1114, 4}, {1115, 4},
    {0, 1}, {1116, 2}, {0, 1}, {1117, 2}, {1118, 4}, {1119, 4}, {1120, 2}, {1121, 7},
    {1122, 4}, {0, 1}, {1123, 2}, {1124, 4}, {1125, 4}, {1126, 2}, {1127, 4}, {1128, 4},
    {1129, 4}, {1130, 4}, {1131, 4}, {1132, 4}, {1133, 4}, {1134, 4}, {1135, 4},
};

constexpr NormalFrame intro_trip[] = { // FOIT move 526
    {0, 1}, {1136, 47}, {0, 1}, {1136, 5}, {1137, 4}, {0, 1}, {1138, 3}, {1139, 5},
    {1140, 5}, {1141, 5}, {0, 1}, {1142, 4}, {1143, 5}, {0, 1}, {1144, 5}, {1145, 19},
    {1146, 6}, {1147, 6}, {1148, 5}, {0, 1}, {1149, 5}, {1150, 6},
};

constexpr NormalFrame intro_bow[] = { // FOIT move 528
    {0, 1}, {1151, 10}, {1152, 4}, {1153, 4}, {1154, 5}, {0, 1}, {1155, 5}, {1156, 5},
    {1157, 5}, {1158, 5}, {1159, 36}, {1160, 5}, {0, 1}, {1161, 5}, {1162, 5}, {1163, 46},
    {1164, 6}, {1165, 6}, {1166, 5}, {1167, 5}, {1168, 6}, {1169, 6},
};

constexpr DefenseClip filia_intros[] = {intro_hat, intro_trip, intro_bow};

// Move 528 spawns child 11 at Filia's origin on record 13. The child sets
// its horizontal scale to zero on its last record.
constexpr unsigned intro_bow_hat_tick = DefenseClip(intro_bow).record_tick(13);
constexpr NormalFrame intro_bow_hat[] = {{1201, 3}, {1202, 3}, {1203, 4}};

struct ResultClips { DefenseClip start, loop; };
constexpr ResultClips filia_wins[] = {
    {win_hair_blown, win_hair_blown_loop}, {win_sassy, win_sassy_loop},
    {win_filia_blown, win_filia_blown_loop},
};
constexpr ResultClips filia_loss{lose_pull_hair, lose_pull_hair_loop};

}
