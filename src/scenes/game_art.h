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
// The land's light (0 plains .. 5 the Heap, -1 none): a gentle tint on every
// character drawn after it, so they sit in the painting.
void SetLandLight(int which);
// A soft shadow on the ground under a character; lift 0 on the ground .. 1 high up.
void DrawShadow(Vector2 ground, float width, float lift);
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
// NULL Ghost, the floating enemy of the Pointer Peaks. at: the point under it.
// solid: 1 normal .. 0 "NULL" (faded: it can't be hit, and it doesn't hurt).
void DrawGhost(Vector2 at, float scale, int face, float hurt, float alpha, float solid, float time);
// The Wild Pointer Wyvern, the Peaks' boss. face: 1 looks right; tilt in degrees
// (a dive leans it); wing: 0..1 through a wing beat.
void DrawWyvern(Vector2 center, float scale, int face, float tilt, float wing, float hurt, float alpha, float time);
// Its aim (a dashed line to a target marked *p; lock 0..1) and the bolt it fires.
void DrawAimLine(Vector2 from, Vector2 to, float lock, float time);
void DrawBolt(Vector2 at, Vector2 dir, float time);
// Leak Slime, the Memory Forest's enemy: it grows (size) as it leaks; hop 0..1
// flattens it before a hop; bytes is what its badge says it has malloc'd.
void DrawSlime(Vector2 feet, float scale, int face, float hop, float size, float hurt, float alpha, int bytes, float time);
// A puddle of leaked bytes it leaves (life 1 new .. 0 gone): stepping in it hurts.
void DrawPuddle(float x, float life, float time);
// The Leak Lord, the Forest's boss. mouth 0..1 (spitting), melt 0..1 (beaten).
void DrawLeakLord(Vector2 feet, float scale, float time, float mouth, float hurt, float alpha, float melt);
// A blob it spits, arcing through the air.
void DrawBlob(Vector2 at, float time);
// Lost Node, Linked List City's enemy: a box [value|next] on legs. Nodes walk in
// lines; head marks the first, tail draws its next as NULL. NodeNextDot is where
// its next pointer is, NodeInlet where an arrow to it arrives (DrawLink joins them).
void DrawNode(Vector2 feet, float scale, int face, float walk, float hurt, float alpha, int value, bool head, bool tail, float time);
Vector2 NodeNextDot(Vector2 feet, float scale, int face);
Vector2 NodeInlet(Vector2 feet, float scale, int face);
void DrawLink(Vector2 from, Vector2 to, float alpha, float time);
// The Tangled List, the City's boss: a spider made of a list. pose 0..1 (rearing
// up to attack), fall 0..1 (coming undone).
void DrawTangledList(Vector2 feet, float scale, float time, float pose, float hurt, float alpha, float fall);
// Its traverse (nodes on the street, the pointer p hopping along them) and its whip.
void DrawListTile(float x, int value, float warn, float struck, float time);
void DrawCursorP(Vector2 tip, float time);
void DrawChainWhip(Vector2 groundPos, float time);
// Princess Ada and the SEGFAULT, for the story.
void DrawAda(Vector2 feet, float scale, float time);
void DrawSegfault(Vector2 center, float scale, float time, float hurt = 0, float alpha = 1);
// Dangling Bat, the Heap's enemy: a bat still pointing at freed memory.
void DrawBat(Vector2 at, float scale, int face, float flap, float hurt, float alpha, float time);
// The SEGFAULT's attacks: where an error column will fall (warn 0..1), the column
// (life 1..0), the broken pointer it throws, and the blue screen sweeping the arena.
void DrawBeamMark(float x, float warn, float time);
void DrawBeam(float x, float life, float time);
void DrawSpear(Vector2 at, float dir, float time);
void DrawBsodWave(float x, float time);
// The Kingdom of Memory, for the story.
void DrawCastle(Rectangle r, float time);
// The terminal the hero uses to fight a boss.
void DrawTerminalPillar(Vector2 feet, float time, bool ready, bool near);

// Where the pictures the game uses are (resources/backgrounds, one per land);
// each is loaded the first time it is drawn. Call with a window open.
void LoadArt(const std::string& resourceDir);
// A land's painting (0 plains .. 3 forest, 4 city, 5 the Heap) filling r, drifting
// slowly and darkened a little: the story's pictures stand in front of it.
void DrawBackdropIn(Rectangle r, int which, float time);

// The Stack Plains: the painted sky (or hills drawn by hand), floating stack frames and the ground.
// cam: world x at the screen's left edge.
void DrawStackPlains(float cam, float time);
// The Array Canyon: a painted desert (or mesas drawn by hand), floating arrays and sandstone.
void DrawArrayCanyon(float cam, float time);
// The Pointer Peaks: painted snowy peaks (or drawn by hand), floating pointers, snow and ice.
void DrawPointerPeaks(float cam, float time);
// The Memory Forest: a painted dark wood, leaked blocks drifting, glowing bits, moss and mushrooms.
void DrawMemoryForest(float cam, float time);
// Linked List City: a painted city at sunset, lists drifting over the roofs, cobbles and lamp posts.
void DrawListCity(float cam, float time);
// The Heap: a painted violet abyss, freed blocks drifting up, glitches, a floor of crystal blocks.
void DrawTheHeap(float cam, float time);

// The game's icon for the desktop.
void DrawGameIcon(Rectangle r, float time, float hot);

} // namespace fp
