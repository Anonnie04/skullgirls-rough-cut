#pragma once

#include <array>

namespace skullgirls {

// Filia FOIT moves 88 and 90. Zero sprite references retain the last pose.

struct MovementFrame {
    unsigned sprite;
    unsigned ticks;
    float velocity;
};

constexpr std::array air_dash_forward{
    MovementFrame{190, 4, 17}, MovementFrame{191, 2, 17}, MovementFrame{192, 2, 17},
    MovementFrame{193, 2, 17}, MovementFrame{191, 2, 17}, MovementFrame{192, 2, 17},
    MovementFrame{193, 2, 17}, MovementFrame{191, 2, 17}, MovementFrame{192, 2, 17},
    MovementFrame{193, 2, 17}, MovementFrame{190, 2, 8}, MovementFrame{190, 1, 0}};
constexpr std::array air_dash_backward{
    MovementFrame{194, 4, -17}, MovementFrame{195, 2, -17}, MovementFrame{196, 2, -17},
    MovementFrame{197, 2, -17}, MovementFrame{195, 2, -17}, MovementFrame{196, 2, -17},
    MovementFrame{197, 2, -9}, MovementFrame{195, 2, -9}, MovementFrame{196, 2, -9},
    MovementFrame{197, 2, -9}, MovementFrame{195, 2, -9}, MovementFrame{194, 3, -4},
    MovementFrame{194, 2, 0}};

// Filia FOIT moves 27, 29, 30, 36 and 40. Sprite references are one-based.
// The preview folds script-only ticks into adjacent poses. Velocities
// are relative to the fighter's facing.
constexpr std::array walk_forward{
    MovementFrame{59, 4, 6}, MovementFrame{60, 4, 6}, MovementFrame{61, 3, 6},
    MovementFrame{62, 4, 6}, MovementFrame{63, 3, 6}, MovementFrame{64, 4, 6},
    MovementFrame{65, 3, 6}, MovementFrame{66, 5, 6}, MovementFrame{67, 3, 6},
    MovementFrame{68, 5, 6}, MovementFrame{69, 3, 6}, MovementFrame{70, 4, 6}};
constexpr std::array walk_backward{
    MovementFrame{71, 4, -4.5f}, MovementFrame{72, 4, -4.5f}};
constexpr std::array walk_backward_loop{
    MovementFrame{73, 4, -4.5f}, MovementFrame{74, 3, -4.5f}, MovementFrame{75, 3, -4.5f},
    MovementFrame{76, 3, -4.5f}, MovementFrame{77, 4, -4.5f}, MovementFrame{78, 3, -4.5f},
    MovementFrame{79, 3, -4.5f}, MovementFrame{80, 3, -4.5f}, MovementFrame{81, 3, -4.5f},
    MovementFrame{82, 3, -4.5f}, MovementFrame{83, 3, -4.5f}, MovementFrame{84, 3, -4.5f},
    MovementFrame{85, 3, -4.5f}, MovementFrame{86, 3, -4.5f}, MovementFrame{87, 4, -4.5f},
    MovementFrame{88, 3, -4.5f}, MovementFrame{89, 3, -4.5f}, MovementFrame{90, 3, -4.5f},
    MovementFrame{91, 3, -4.5f}, MovementFrame{92, 3, -4.5f}};
constexpr std::array dash_forward{
    MovementFrame{93, 4, 12}, MovementFrame{94, 2, 15}, MovementFrame{95, 2, 15},
    MovementFrame{96, 2, 15}, MovementFrame{97, 2, 15}, MovementFrame{95, 2, 15},
    MovementFrame{96, 2, 15}, MovementFrame{97, 2, 15}, MovementFrame{95, 2, 15},
    MovementFrame{96, 4, 15}, MovementFrame{94, 4, 14}};
constexpr std::array dash_backward{
    MovementFrame{107, 4, -12}, MovementFrame{108, 2, -12}, MovementFrame{109, 2, -17},
    MovementFrame{110, 2, -17}, MovementFrame{111, 2, -17}, MovementFrame{109, 2, -10},
    MovementFrame{110, 2, -10}, MovementFrame{111, 2, -10}, MovementFrame{109, 2, -10},
    MovementFrame{112, 3, -6}, MovementFrame{112, 3, -2}};

}
