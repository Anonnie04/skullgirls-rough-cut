#pragma once
namespace skullgirls {
// Temporary story-preview AI script. Called for player 2 on every combat tick.
// Intentionally issues no movement, attacks, blocking, tags, or menu inputs.
// Replace this function with the native arcade AI interpreter in a later pass.
struct StoryEnemyInput { unsigned held=0, pressed=0; };
inline StoryEnemyInput story_enemy_ai() { return {}; }
}
