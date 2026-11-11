// ============================================================================
// game_art.h  -  The Forgotten Pointer: everything the game draws, made of
// raylib shapes (no image files). Positions are canvas units; "feet" is the
// point a character stands on.
// ============================================================================
#pragma once
#include "raylib.h"

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
// Princess Ada and the SEGFAULT, for the story.
void DrawAda(Vector2 feet, float scale, float time);
void DrawSegfault(Vector2 center, float scale, float time);
// The Kingdom of Memory, for the story.
void DrawCastle(Rectangle r, float time);
// The terminal the hero uses to fight a boss.
void DrawTerminalPillar(Vector2 feet, float time, bool ready, bool near);

// The Stack Plains: sky, parallax hills, floating stack frames and the ground.
// cam: world x at the screen's left edge.
void DrawStackPlains(float cam, float time);

// The game's icon for the desktop.
void DrawGameIcon(Rectangle r, float time, float hot);

} // namespace fp
