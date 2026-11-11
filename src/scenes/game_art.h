// ============================================================================
// game_art.h  -  The Forgotten Pointer: everything the game draws, made of
// raylib shapes (no image files). Positions are canvas units; "feet" is the
// point a character stands on.
// ============================================================================
#pragma once
#include "raylib.h"
#include <string>

namespace fp {

constexpr float kGround = 650;   // the y of the ground in the levels

// The hero, the Debugger. face: 1 right, -1 left. walk: phase of the steps.
// attack: -1 when not attacking, else 0..1 through the swing.
void DrawHero(Vector2 feet, float scale, int face, float walk, float attack, float time, bool blink);
// Off-by-One Goblin, the weak enemy of the Stack Plains.
void DrawGoblin(Vector2 feet, float scale, int face, float walk, float hurt, float alpha);
// Infinite Loop Serpent: a snake biting its tail, spinning. hurt: 0..1 flash.
void DrawSerpent(Vector2 center, float radius, float time, float hurt, float alpha);
// The loop ring the Serpent spits along the ground.
void DrawLoopRing(Vector2 center, float radius, float time);
// Out-of-Bounds Imp, the hopping enemy of the Array Canyon. hop: 0 on the ground,
// more in the air. index: the out-of-bounds index shown over its head.
void DrawImp(Vector2 feet, float scale, int face, float hop, float hurt, float alpha, int index, float time);
// The Matrix Golem, the Array Canyon's boss. pose: 0 arms down .. 1 raised;
// crumble: 0 whole .. 1 fallen apart.
void DrawGolem(Vector2 feet, float scale, float time, float pose, float hurt, float alpha, float crumble);
// A block the Golem drops, the mark where it will land, and its slam's shockwave.
void DrawStoneBlock(Vector2 center, float size, int value, float alpha);
void DrawDropMarker(float x, float time, float progress, const char* label);
void DrawShockwave(Vector2 groundPos, float time);
// Princess Ada and the SEGFAULT, for the story.
void DrawAda(Vector2 feet, float scale, float time);
void DrawSegfault(Vector2 center, float scale, float time);
// The Kingdom of Memory, for the story.
void DrawCastle(Rectangle r, float time);
// The terminal the hero uses to fight a boss.
void DrawTerminalPillar(Vector2 feet, float time, bool ready, bool near);

// Loads the pictures the game uses (resources/backgrounds): one per land. Call once, with a window open.
void LoadArt(const std::string& resourceDir);

// The Stack Plains: the painted sky (or hills drawn by hand), floating stack frames and the ground.
// cam: world x at the screen's left edge.
void DrawStackPlains(float cam, float time);
// The Array Canyon: a painted desert (or mesas drawn by hand), floating arrays and sandstone.
void DrawArrayCanyon(float cam, float time);

// The game's icon for the desktop.
void DrawGameIcon(Rectangle r, float time, float hot);

} // namespace fp
