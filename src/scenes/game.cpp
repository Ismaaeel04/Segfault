// ============================================================================
// game.cpp  -  The Forgotten Pointer: a side-scrolling adventure on the
// SegFault OS desktop. Walk through each land, and when you strike a bug a
// question may appear: answer it right for a critical hit, wrong and the bug
// strikes back. Each land ends with a boss you beat by writing code: every
// test your code passes is a hit.
//
// Chapter 1: Stack Plains (Off-by-One Goblins, the Infinite Loop Serpent).
// Chapter 2: Array Canyon (Out-of-Bounds Imps, the Matrix Golem).
// Chapter 3: Pointer Peaks (NULL Ghosts, the Wild Pointer Wyvern).
// Chapter 4: Memory Forest (Leak Slimes, the Leak Lord).
// Chapter 5: Linked List City (Lost Nodes walking in lines, the Tangled List).
// Chapter 6: The Heap (Dangling Bats, the SEGFAULT), then the ending and the credits.
// Once a land is reached, Chapters on the menu plays it again.
//
// Game feel: coyote time and a jump buffer, shorter hops when jump is let go
// early, a dash with a moment of invulnerability, hit-stop, screen shake that
// fades (it can be switched off), particles, squash and stretch, combos and a
// camera that looks ahead.
// Plus the title menu, settings, and the story between the lands.
// ============================================================================
#include "app/app.h"
#include "app/sound.h"
#include "app/theme.h"
#include "app/ui.h"
#include "content/content.h"
#include "scenes/game_art.h"
#include "scenes/workspace.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <vector>

using namespace ui;
using fp::kGround;

namespace {

Color Hex(unsigned v, unsigned char a = 255) { return {(unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, a}; }
float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float Rand01() { return GetRandomValue(0, 10000) / 10000.0f; }

// --------------------------------------------------------------- settings ---
enum Action { ACT_LEFT, ACT_RIGHT, ACT_JUMP, ACT_ATTACK, ACT_USE, ACT_DASH, ACT_COUNT };
const char* kActionName[ACT_COUNT] = {"Move left", "Move right", "Jump", "Attack", "Use terminal", "Dash"};
const int kDefaultKeys[ACT_COUNT] = {KEY_A, KEY_D, KEY_SPACE, KEY_J, KEY_E, KEY_LEFT_SHIFT};

std::string KeyName(int k) {
    if (k >= KEY_A && k <= KEY_Z) return std::string(1, (char)k);
    if (k >= KEY_ZERO && k <= KEY_NINE) return std::string(1, (char)k);
    switch (k) {
        case KEY_SPACE: return "Space";
        case KEY_LEFT: return "Left";
        case KEY_RIGHT: return "Right";
        case KEY_UP: return "Up";
        case KEY_DOWN: return "Down";
        case KEY_ENTER: return "Enter";
        case KEY_TAB: return "Tab";
        case KEY_LEFT_SHIFT: return "Shift";
        case KEY_RIGHT_SHIFT: return "R-Shift";
        case KEY_LEFT_CONTROL: return "Ctrl";
        case KEY_LEFT_ALT: return "Alt";
        case KEY_COMMA: return ",";
        case KEY_PERIOD: return ".";
        case KEY_SLASH: return "/";
        case KEY_SEMICOLON: return ";";
    }
    return "Key " + std::to_string(k);
}

struct Settings {
    int keys[ACT_COUNT] = {KEY_A, KEY_D, KEY_SPACE, KEY_J, KEY_E, KEY_LEFT_SHIFT};
    bool shake = true;           // screen shake (off for anyone it bothers)
    int difficulty = 1;          // 0 easy (7 hearts), 1 normal (5), 2 hard (3)
    bool questionTimer = true;   // 25 s to answer
    float volume = 0.6f;         // sound effects
    float music = 0.5f;          // 0 = no music
    int Hearts() const { return difficulty == 0 ? 7 : difficulty == 1 ? 5 : 3; }
};

// ------------------------------------------------------------- the world ---
struct Hero {
    float x = 200, y = kGround, vy = 0;
    int face = 1;
    float walk = 0, attack = -1, attackCd = 0, hurt = 0;
    float dash = 0, dashCd = 0, invuln = 0;      // dashing (time left), until the next dash, can't be hurt
    float coyote = 0, jumpBuf = 0, squash = 0;   // jump forgiveness; squash < 0 flattened, > 0 stretched
    int hearts = 5;
    bool grounded = true, jumping = false;
};

struct Goblin {
    float x, home;
    int face = -1;
    int hp = 3, maxHp = 3;
    float walk = 0, hurt = 0, dying = -1, touchCd = 0;
    float hy = 0, vy = 0, hopCd = 0.5f;   // imps: height above the ground, fall speed, time to the next hop
                                           // ghosts: hy their height, vy how solid (1) or NULL (0), hopCd time to the next change
    int index = 4;                         // imps: the out-of-bounds index they carry; ghosts: 1 solid, 0 turning NULL
                                           // slimes: vy their size, index the bytes leaked, hopCd time to the next leak
    bool alive = true;
    bool minion = false;                   // spawned by a boss: one hit, no question
    int group = -1, order = 0;             // nodes: which line they walk in, and their place in it
};

struct Ring { float x, vx; float life; };   // the Serpent's loop rings, the Golem's shockwaves

// A block the Golem drops: first a mark where it will land, then it falls.
struct Rock { float x, y, vy, warn, warn0; int value; std::string label; float landed; };

// A bolt the Wyvern fires along its aim (and the Leak Lord's blobs, which fall).
struct Bolt { Vector2 p, v; };

// A puddle of leaked memory on the ground.
struct Puddle { float x, life; };

// A node the Tangled List lays on the street for its traverse.
struct Tile { float x; int value; float struck; };

// A column of crash the SEGFAULT brings down: a mark first, then the beam.
struct Beam { float x, warn, warn0, life; };

// A particle: dust, sparks, the bits an enemy bursts into.
struct Particle { float x, y, vx, vy, life, max, size, grav; Color c; int kind; };   // kind: 0 puff, 1 square, 2 streak

struct Popup { std::string text; float x, y, age; Color col; };

enum State { MENU, NEW_GAME, SETTINGS, STORY, CHAPTER, PLAY, QUIZ, TERMINAL, PAUSE, DEAD, VICTORY, CONFIRM_RESTART, CHAPTERS, CREDITS };

const float kLevelLen = 5200;

// The lands, one per exam topic. Chapter n is kLands[n - 1].
struct Land {
    const char* name;
    const char* topic;
    const char* boss;        // its full name
    const char* bossShort;   // "the Serpent"
    const char* enemy;       // one of its bugs
    const char* enemies;
    const char* music;
    const char* bossMusic;
    Section a, b;            // the questions and the boss's problem come from these sections
    unsigned bossColor;
};
const Land kLands[] = {
    {"Stack Plains", "Loops and numbers", "Infinite Loop Serpent", "the Serpent", "goblin", "goblins", "plains", "boss", S_NUMBERS, S_LOOPS, 0x3fd1c7},
    {"Array Canyon", "Arrays and 2D arrays", "Matrix Golem", "the Golem", "imp", "imps", "canyon", "golem", S_ARRAYS, S_GRID, 0xff9e4a},
    {"Pointer Peaks", "Pointers and pointer arithmetic", "Wild Pointer Wyvern", "the Wyvern", "ghost", "ghosts", "peaks", "wyvern", S_POINTERS, S_PTRARITH, 0x9fd8ff},
    {"Memory Forest", "Memory management", "Leak Lord", "the Leak Lord", "slime", "slimes", "forest", "leak", S_MEMORY, S_MEMORY, 0x7be36b},
    {"Linked List City", "Linked lists and strings", "Tangled List", "the Tangled List", "node", "nodes", "city", "tangle", S_LISTS, S_STRINGS, 0x5fd0e0},
    // the last land is the final exam: its questions come from every section
    {"The Heap", "Everything: the final exam", "SEGFAULT", "the SEGFAULT", "bat", "bats", "heap", "segfault", S_POINTERS, S_MEMORY, 0xff3d5a},
};

// What a beaten bug bursts into, per land.
const unsigned kBugColor[] = {0x6fd15a, 0xff6b4a, 0xd8f0ff, 0x7be36b, 0x2f9db0, 0x7a58a8};
const int kLandCount = (int)(sizeof(kLands) / sizeof(kLands[0]));

// ---------------------------------------------------------------- story ----
struct Panel { const char* text; int art; };
const Panel kIntro[] = {
    {"Deep inside the machine lies the Kingdom of Memory, where every byte has its place.", 0},
    {"Princess Ada keeps the Kernel running. While she watches over it, no program ever crashes.", 1},
    {"One night the SEGFAULT, a monster born from a single forgotten pointer, broke through the stack. "
     "It took Ada and fled into the depths of the Heap.", 2},
    {"Every program began to crash. You are the Debugger, the last knight who still reads code. "
     "Cross six lands, defeat their guardians, and bring Ada back before the Kingdom crashes for good.", 3},
};
const Panel kAfterChapter1[] = {
    {"The Infinite Loop Serpent lets go of its own tail, and its endless circle breaks at last.", 4},
    {"Among the scales lies a ribbon from Ada's crown. Her trail leads east, out of the plains and into the Array Canyon.", 1},
    {"There the stones stand in rows and columns, each with its index. Imps hop between them, always reaching one past the end, "
     "and at the far wall waits the Matrix Golem.", 5},
};
const Panel kAfterChapter2[] = {
    {"The Matrix Golem falls apart, row by row, until only loose stones are left.", 6},
    {"In its heart lies a second ribbon, and an address scratched into the stone. It points north, up into the Pointer Peaks.", 1},
    {"Up there nothing is what it seems: ghosts that point at nothing, and a wyvern born from a pointer that nobody ever set.", 7},
};
const Panel kAfterChapter3[] = {
    {"The Wild Pointer Wyvern crashes into the snow. Its arrow of a tail finally points at nothing at all.", 8},
    {"A third ribbon lies frozen in the ice. Below the peaks the path falls into a forest where memory leaks away...", 1},
    {"Every block malloc'd and never freed ends up here. The slimes grow fat on it, and the fattest of all wears a crown: the Leak Lord.", 9},
};
const Panel kAfterChapter4[] = {
    {"free(), at last. The Leak Lord melts into the moss, and the forest breathes again.", 10},
    {"Under the slime lies the fourth ribbon, and beyond the trees, the lights of a city where everything is linked...", 1},
    {"In Linked List City every house points to the next one. In the square, knotted into itself, waits the Tangled List.", 11},
};
const Panel kAfterChapter5[] = {
    {"The Tangled List comes undone, node by node, until nothing is left but NULL.", 12},
    {"The fifth ribbon hangs from its last pointer. Under the square, the ground opens into the Heap itself...", 1},
    {"Down there waits the SEGFAULT, and Ada with it. One last land, one last guardian: everything you have learned.", 2},
};
const Panel kAfterChapter6[] = {
    {"The SEGFAULT shudders. Its screen flickers one last time, 0xC0000005... and then, at last, it simply exits.", 15},
    {"In the deepest block of the Heap, Ada is free. \"You read every line,\" she says. \"Even the ones that crashed.\"", 13},
    {"Back in the Kingdom of Memory every byte finds its place again, and every program runs to its end.", 0},
    {"Every pointer remembered. Every block freed. The Forgotten Pointer has been found.", 14},
};

// ================================================================= scene ===
class GameScene : public Scene {
public:
    const char* Title() const override { return "The Forgotten Pointer"; }
    bool AiAllowed() const override { return false; }

    void Enter(const std::string& param) override {
        LoadSave();
        fp::LoadArt(app.resourceDir);
        state = MENU;
        menuSel = 0;
        t = 0;
        snd::SetMusicVolume(set.music);
        snd::PlayMusic("title");
        for (const char* id : {"story", "plains", "boss", "canyon", "golem", "peaks", "wyvern", "forest", "leak", "city", "tangle", "heap", "segfault", "gameover"}) snd::Preload(id);   // ready before they are needed
        // --shot hooks, to check each screen from the command line ("play2", "boss3": the same in chapter 2, 3)
        std::string p = param;
        land = 0;
        if (!p.empty() && p.back() >= '2' && p.back() <= '9') { land = std::min(p.back() - '1', kLandCount - 1); p.pop_back(); }
        if (p == "settings") state = SETTINGS;
        else if (p == "story" && land == 0) StartStory(kIntro, 4, CHAPTER), panelIdx = 2, panelT = 100;
        else if (p == "story" && land == 1) StartStory(kAfterChapter1, 3, CHAPTER, 1), panelIdx = 2, panelT = 100;
        else if (p == "story" && land == 2) StartStory(kAfterChapter2, 3, CHAPTER, 2), panelIdx = 2, panelT = 100;
        else if (p == "story" && land == 3) StartStory(kAfterChapter3, 3, CHAPTER, 3), panelIdx = 2, panelT = 100;
        else if (p == "story" && land == 4) StartStory(kAfterChapter4, 3, CHAPTER, 4), panelIdx = 2, panelT = 100;
        else if (p == "story") StartStory(kAfterChapter5, 3, CHAPTER, 5), panelIdx = 2, panelT = 100;
        else if (p == "ending") StartStory(kAfterChapter6, 4, CREDITS), panelIdx = 1, panelT = 100;
        else if (p == "ending2") StartStory(kAfterChapter6, 4, CREDITS), panelIdx = 3, panelT = 100;
        else if (p == "credits") { state = CREDITS; creditsT = 9; }
        else if (p == "chapters") { chaptersDone = 4; state = CHAPTERS; }
        else if (p == "chapter") StartChapter(land), chapterT = 0.5f;
        else if (p == "play" || p == "quiz" || p == "boss" || p == "terminal") {
            ResetLevel();
            state = PLAY;
            snd::PlayMusic(kLands[land].music);
            if (p == "quiz") { hero.x = 900; AskQuestion(0, false); }
            if (p == "play" && land >= 1) { hero.x = 900; for (int i = 0; i < 40; i++) Update(1 / 60.0f); }   // the imps hopping, the ghosts
            if (p == "boss" || p == "terminal") {
                goblins.clear(); hero.x = kLevelLen - 600; Update(0.016f); bossT = 2; ringCd = 0; golemCd = 0; wyCd = 0; lordCd = 0; tlCd = 0; sfCd = 0; Update(0.5f);
                for (int i = 0; i < 45; i++) Update(1 / 60.0f);   // the Golem's first attack under way
            }
            if (p == "terminal") state = TERMINAL;
        }
    }

    void Frame(float dt) override {
        t += dt;
        switch (state) {
            case MENU: Menu(dt); break;
            case NEW_GAME: Menu(dt); NewGameSetup(); break;
            case CONFIRM_RESTART: Menu(dt); ConfirmRestart(); break;
            case SETTINGS: SettingsScreen(); break;
            case STORY: Story(dt); break;
            case CHAPTER: Chapter(dt); break;
            case PLAY:   // hit-stop: the world holds still for a moment on a big hit
                if (hitStop > 0) hitStop -= dt;
                else Update(dt);
                Draw();
                break;
            case QUIZ: Draw(); Quiz(dt); break;
            case TERMINAL: Terminal(); break;
            case PAUSE: Draw(); Pause(); break;
            case DEAD: Draw(); Dead(dt); break;
            case VICTORY: Victory(dt); break;
            case CHAPTERS: Chapters(); break;
            case CREDITS: Credits(dt); break;
        }
    }

private:
    State state = MENU, settingsBack = MENU;
    float t = 0;
    Settings set;
    int chaptersDone = 0;   // saved progress
    int menuSel = 0;
    // settings screen
    int rebinding = -1, setSel = 0;
    // story
    const Panel* panels = nullptr;
    int panelCount = 0, panelIdx = 0;
    float panelT = 0;
    State afterStory = CHAPTER;
    int storyLand = 0;      // the chapter a story leads to
    float chapterT = 0;
    int land = 0;           // the chapter being played (0 = Stack Plains)
    // play
    Hero hero;
    std::vector<Goblin> goblins;
    std::vector<Ring> rings;
    std::vector<Rock> rocks;
    std::vector<Bolt> bolts;
    std::vector<Bolt> blobs;          // the Leak Lord's
    std::vector<Puddle> puddles;
    std::vector<Particle> parts;
    float hitStop = 0, hurtFlash = 0, look = 0;   // freeze frames, the red edge when hurt, the camera's look-ahead
    int combo = 0;
    float comboT = 0, comboPop = 0;
    // the Leak Lord: what it is doing (0 waiting, 1 spitting, 2 calling slimes)
    float lordCd = 2, lordT = 0, lordMouth = 0;
    int lordAct = 0;
    // the Tangled List: what it is doing (0 waiting, 1 traversing, 2 whipping), where p is
    std::vector<Tile> tiles;
    float tlCd = 2, tlT = 0, tlPose = 0;
    int tlAct = 0, tlP = -1;
    // the SEGFAULT: what it is doing (0 waiting, 1 crash columns, 2 the spear, 3 the blue screen)
    std::vector<Beam> beams;
    std::vector<float> walls;          // blue screens sweeping the arena
    float sfCd = 2, sfT = 0;
    int sfAct = 0;
    bool dashHint = false;             // "dash through it" has been said
    float creditsT = 0;
    int chapSel = 0;
    std::vector<Popup> popups;
    float cam = 0, shake = 0;
    bool arena = false, bossAwake = false;
    float arenaCam = 0, termX = 0;
    float bossHp = 1, bossHurt = 0, bossX = 0, bossT = 0, ringCd = 2, termCd = 0, bossDying = -1;
    // the Golem: its arms (0 down .. 1 raised), what it is winding up (0 nothing, 1 a rain of blocks, 2 a slam)
    float golemPose = 0, golemCd = 2, golemT = 0;
    int golemAct = 0;
    // the Wyvern: where it flies (bossX, bossY), what it is doing (0 hovering, 1 aiming, 2 diving, 3 flying back)
    float bossY = 250, wyCd = 2, wyT = 0, wyTilt = 0, wyDiveX0 = 0, wyDiveY0 = 0;
    int wyAct = 0, wyFace = -1;
    Vector2 aimTo = {0, 0};
    // quiz
    std::vector<int> questionPool;
    size_t poolNext = 0;
    const Question* q = nullptr;
    std::vector<int> order;            // shuffled options; order[i] = index into q->options
    int picked = -1, quizTarget = -1;
    bool quizFinal = false, quizRight = false;
    float quizT = 0, answeredAt = 0;
    // boss terminal
    Workspace ws;
    const Problem* bossProblem = nullptr;
    bool wsRunning = false;
    int bestPassed = 0;
    std::string termMsg;
    float deadT = 0;

    void Fx(snd::Fx f, float scale = 1) const { snd::PlayFx(f, set.volume * scale); }

    // ------------------------------------------------------------ saving --
    std::string SavePath() const {
        return (app.userDir.empty() ? app.dataDir : app.userDir) + "forgotten_pointer.txt";
    }
    void LoadSave() {
        std::ifstream in(SavePath());
        std::string k;
        while (in >> k) {
            if (k == "chapters") in >> chaptersDone;
            else if (k == "difficulty") in >> set.difficulty;
            else if (k == "timer") in >> set.questionTimer;
            else if (k == "volume") in >> set.volume;
            else if (k == "music") in >> set.music;
            else if (k == "shake") in >> set.shake;
            else if (k == "key") { int a, v; in >> a >> v; if (a >= 0 && a < ACT_COUNT && v > 0) set.keys[a] = v; }
        }
        set.difficulty = std::clamp(set.difficulty, 0, 2);
        set.volume = Clamp01(set.volume);
        set.music = Clamp01(set.music);
    }
    void Save() const {
        std::ofstream out(SavePath());
        out << "chapters " << chaptersDone << "\n";
        out << "difficulty " << set.difficulty << "\n";
        out << "timer " << (set.questionTimer ? 1 : 0) << "\n";
        out << "volume " << set.volume << "\n";
        out << "music " << set.music << "\n";
        out << "shake " << (set.shake ? 1 : 0) << "\n";
        for (int a = 0; a < ACT_COUNT; a++) out << "key " << a << " " << set.keys[a] << "\n";
    }

    // ------------------------------------------------------------- input --
    bool Down(Action a) const {
        if (IsKeyDown(set.keys[a])) return true;
        if (a == ACT_LEFT) return IsKeyDown(KEY_LEFT);
        if (a == ACT_RIGHT) return IsKeyDown(KEY_RIGHT);
        if (a == ACT_JUMP) return IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
        return false;
    }
    bool Pressed(Action a) const {
        if (IsKeyPressed(set.keys[a])) return true;
        if (a == ACT_JUMP) return IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
        if (a == ACT_ATTACK) return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (a == ACT_DASH) return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        return false;
    }

    // ---------------------------------------------------- shared drawing --
    // A menu entry in the game's style; returns true when chosen.
    bool Item(float cx, float y, const std::string& label, bool selected, bool enabled = true, float w = 360) {
        float h = 54;
        Rectangle r = {cx - w / 2, y, w, h};
        bool hover = enabled && MouseIn(r);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { Fx(snd::FX_MENU_SELECT); return true; }
        bool on = enabled && (selected || hover);
        if (on) {
            DrawRectangleRounded(r, 0.5f, 12, Fade(Hex(0x8b7bff), 0.28f));
            DrawRectangleRoundedLinesEx(r, 0.5f, 12, 2, Hex(0xb9b0ff));
            // a small pointer arrow, of course
            Vector2 a = {r.x + 26, r.y + h / 2};
            DrawTriangle({a.x + 12, a.y}, {a.x - 4, a.y - 9}, {a.x - 4, a.y + 9}, Hex(0xff9e4a));
        }
        TextCenter(label, r, 22, enabled ? (on ? WHITE : Hex(0xd8d3f5)) : Hex(0x6b648f), F_BOLD);
        if (hover) WantHand();
        return false;
    }

    void Shade(float a) { DrawRectangleRec(ScreenRect(), Fade(Hex(0x07060d), a)); }

    void TitleBackdrop() {
        float drift = (0.5f - 0.5f * std::cos(t * 0.05f)) * 2600;   // drifts back and forth
        int far = std::min(chaptersDone, kLandCount - 1);   // the land you would continue in
        fp::SetLandLight(far);
        if (far == 5) fp::DrawTheHeap(drift, t);
        else if (far == 4) fp::DrawListCity(drift, t);
        else if (far == 3) fp::DrawMemoryForest(drift, t);
        else if (far == 2) fp::DrawPointerPeaks(drift, t);
        else if (far == 1) fp::DrawArrayCanyon(drift, t);
        else fp::DrawStackPlains(drift, t);
        // the hero, standing at the edge of the plains
        fp::DrawShadow({ScreenRect().x + 220, kGround}, 52, 0);
        fp::DrawHero({ScreenRect().x + 220, kGround}, 1.3f, 1, 0, -1, t, false);
    }

    // -------------------------------------------------------------- menu --
    void Menu(float) {
        TitleBackdrop();
        Rectangle scr = ScreenRect();   // darker at the top, where the title is
        DrawRectangleGradientV((int)scr.x, 0, (int)scr.width + 1, (int)H, Fade(Hex(0x120f26), 0.62f), Fade(Hex(0x120f26), 0.18f));
        const float cx = W / 2;
        // the title
        float bob = std::sin(t * 1.3f) * 4;
        const Color shadow = Fade(Hex(0x0b0918), 0.55f);
        TextCenter("THE FORGOTTEN", {3, 113 + bob, W, 50}, 44, shadow, F_HEAVY);
        TextCenter("POINTER", {4, 156 + bob, W, 100}, 92, shadow, F_HEAVY);
        TextCenter("THE FORGOTTEN", {0, 110 + bob, W, 50}, 44, Hex(0xd3ccff), F_HEAVY);
        TextCenter("POINTER", {0, 152 + bob, W, 100}, 92, WHITE, F_HEAVY);
        TextCenter("A SegFault story", {0, 260, W, 24}, 17, Hex(0xe6e1ff), F_MEDIUM);
        if (state != MENU) return;

        struct Entry { const char* label; bool enabled; int id; };
        std::vector<Entry> items;
        if (chaptersDone > 0) items.push_back({"Continue", true, 0});
        if (chaptersDone > 0) items.push_back({"Chapters", true, 5});
        items.push_back({"New Game", true, 1});
        if (chaptersDone > 0) items.push_back({"Restart", true, 2});
        items.push_back({"Settings", true, 3});
        items.push_back({"Quit to desktop", true, 4});
        menuSel = std::clamp(menuSel, 0, (int)items.size() - 1);
        int before = menuSel;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) menuSel = (menuSel + 1) % items.size();
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) menuSel = (menuSel + (int)items.size() - 1) % items.size();
        int chosen = -1;
        for (size_t i = 0; i < items.size(); i++) {
            float y = 340 + i * 64;
            Rectangle r = {cx - 180, y, 360, 54};
            if (MouseIn(r)) menuSel = (int)i;
            if (Item(cx, y, items[i].label, (int)i == menuSel, items[i].enabled)) chosen = items[i].id;
        }
        if (menuSel != before) Fx(snd::FX_MENU_MOVE);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { chosen = items[menuSel].id; Fx(snd::FX_MENU_SELECT); }
        TextCenter("Arrows / mouse to choose, Enter to start", {0, H - 46, W, 20}, 13, Hex(0x8f88b8), F_MEDIUM);

        switch (chosen) {
            case 0: StartChapter(std::min(chaptersDone, kLandCount - 1)); break;   // the next land, or the last one
            case 1:
                if (chaptersDone > 0) { state = CONFIRM_RESTART; break; }
                state = NEW_GAME;
                break;
            case 2: state = CONFIRM_RESTART; break;
            case 3: settingsBack = MENU; state = SETTINGS; rebinding = -1; break;
            case 4: Leave(); break;
            case 5: state = CHAPTERS; chapSel = std::min(chaptersDone, kLandCount - 1); break;
            default: break;
        }
    }

    void NewGameSetup() {
        Shade(0.65f);
        Rectangle box = {W / 2 - 300, H / 2 - 155, 600, 310};
        DrawRectangleRounded(box, 0.10f, 12, Hex(0x1b1736));
        DrawRectangleRoundedLinesEx(box, 0.10f, 12, 2, Hex(0x8b7bff));
        TextCenter("New Game", {box.x, box.y + 24, box.width, 34}, 28, WHITE, F_HEAVY);
        TextCenter("Choose your settings before entering the Kingdom of Memory.",
                   {box.x, box.y + 62, box.width, 20}, 14.5f, Hex(0xb9b3d9), F_MEDIUM);

        const float rx = box.x + 36, rw = box.width - 72, rh = 44;
        const char* diff[3] = {"Easy  (7 hearts)", "Normal  (5 hearts)", "Hard  (3 hearts)"};
        if (Cycle({rx, box.y + 104, rw, rh}, "Difficulty", diff[set.difficulty])) {
            set.difficulty = (set.difficulty + 1) % 3;
            Fx(snd::FX_MENU_MOVE);
            Save();
        }
        if (Cycle({rx, box.y + 158, rw, rh}, "Question timer", set.questionTimer ? "On  (25 s)" : "Off")) {
            set.questionTimer = !set.questionTimer;
            Fx(snd::FX_MENU_MOVE);
            Save();
        }

        bool start = Item(W / 2 - 135, box.y + 228, "Start Game", false, true, 240) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
        bool back = Item(W / 2 + 135, box.y + 228, "Cancel", false, true, 240) || IsKeyPressed(KEY_ESCAPE);
        if (start) {
            chaptersDone = 0;
            Save();
            StartStory(kIntro, 4, CHAPTER, 0);
        } else if (back) {
            state = MENU;
        }
    }

    void ConfirmRestart() {
        Shade(0.6f);
        Rectangle box = {W / 2 - 260, H / 2 - 110, 520, 220};
        DrawRectangleRounded(box, 0.12f, 12, Hex(0x1b1736));
        DrawRectangleRoundedLinesEx(box, 0.12f, 12, 2, Hex(0x8b7bff));
        TextCenter("Start again from the beginning?", {box.x, box.y + 34, box.width, 30}, 22, WHITE, F_BOLD);
        TextCenter("Your progress through the lands is lost.", {box.x, box.y + 74, box.width, 22}, 15, Hex(0xb9b3d9), F_MEDIUM);
        bool yes = Item(W / 2 - 120, box.y + 130, "Yes, restart", false, true, 220) || IsKeyPressed(KEY_Y);
        bool no = Item(W / 2 + 120, box.y + 130, "Cancel", false, true, 220) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_N);
        if (yes) state = NEW_GAME;
        else if (no) state = MENU;
    }

    void Leave() {
        snd::StopMusic();
        if (app.hasReturn) app.Go(app.returnScene, app.returnParam);
        else app.Go(SceneId::Desktop, app.user.empty() ? "login" : "home");
    }

    // ---------------------------------------------------------- settings --
    void SettingsScreen() {
        TitleBackdrop();
        Shade(0.72f);
        TextCenter("Settings", {0, 54, W, 50}, 38, WHITE, F_HEAVY);
        const float x0 = W / 2 - 300, w = 600, rh = 42, step = 47;
        float y = 120;
        // controls
        Text("CONTROLS", x0, y, 13, Hex(0x8f88b8), F_BOLD);
        y += 24;
        for (int a = 0; a < ACT_COUNT; a++) {
            Rectangle row = {x0, y, w, rh};
            bool hover = MouseIn(row);
            DrawRectangleRounded(row, 0.3f, 8, Fade(Hex(0x2a2550), hover ? 0.9f : 0.6f));
            Text(kActionName[a], row.x + 18, row.y + 11, 16.5f, Hex(0xe6e1ff), F_MEDIUM);
            std::string k = rebinding == a ? "press a key..." : KeyName(set.keys[a]);
            if (a == ACT_LEFT) k += rebinding == a ? "" : "  /  Left";
            if (a == ACT_RIGHT) k += rebinding == a ? "" : "  /  Right";
            if (a == ACT_JUMP) k += rebinding == a ? "" : "  /  Up";
            if (a == ACT_ATTACK) k += rebinding == a ? "" : "  /  Mouse";
            if (a == ACT_DASH) k += rebinding == a ? "" : "  /  Right mouse";
            TextRight(k, row.x + row.width - 18, row.y + 11, 16.5f, rebinding == a ? Hex(0xff9e4a) : Hex(0xb9b0ff), F_BOLD);
            if (hover) WantHand();
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && rebinding < 0) rebinding = a;
            y += step;
        }
        if (rebinding >= 0) {
            int k = GetKeyPressed();
            if (k == KEY_ESCAPE) rebinding = -1;
            else if (k > 0) { set.keys[rebinding] = k; rebinding = -1; Save(); }
        }
        y += 14;
        Text("AUDIO", x0, y, 13, Hex(0x8f88b8), F_BOLD);
        y += 24;
        if (Slider({x0, y, w, rh}, "Music", set.music)) { snd::SetMusicVolume(set.music); Save(); }
        snd::SetMusicVolume(set.music);   // follows the bar while it is dragged
        y += step;
        if (Slider({x0, y, w, rh}, "Sound effects", set.volume)) { Save(); Fx(snd::FX_RIGHT); }
        y += step;
        if (Cycle({x0, y, w, rh}, "Screen shake", set.shake ? "On" : "Off")) { set.shake = !set.shake; Save(); Fx(snd::FX_MENU_MOVE); }
        y += 64;
        if (Item(W / 2, y, "Back", false) || (rebinding < 0 && IsKeyPressed(KEY_ESCAPE))) { Save(); state = settingsBack; }
    }

    // A volume bar: drag it. True when the mouse lets go (time to save).
    bool Slider(Rectangle row, const char* label, float& value) {
        DrawRectangleRounded(row, 0.3f, 8, Fade(Hex(0x2a2550), 0.6f));
        Text(label, row.x + 18, row.y + 11, 16.5f, Hex(0xe6e1ff), F_MEDIUM);
        Rectangle bar = {row.x + row.width - 260, row.y + row.height / 2 - 4, 200, 8};
        DrawRectangleRounded(bar, 1, 8, Hex(0x3b3470));
        DrawRectangleRounded({bar.x, bar.y, bar.width * value, bar.height}, 1, 8, Hex(0x8b7bff));
        DrawCircleV({bar.x + bar.width * value, bar.y + 4}, 9, WHITE);
        TextRight(std::to_string((int)std::lround(value * 100)) + "%", row.x + row.width - 18, row.y + 11, 15.5f, Hex(0xb9b0ff), F_BOLD);
        Rectangle hit = {bar.x - 10, row.y, bar.width + 20, row.height};
        if (MouseIn(hit)) WantHand();
        if (MouseIn(hit) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) value = Clamp01((Mouse().x - bar.x) / bar.width);
        return MouseIn(hit) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    }

    bool Cycle(Rectangle row, const char* label, const char* value) {
        bool hover = MouseIn(row);
        DrawRectangleRounded(row, 0.3f, 8, Fade(Hex(0x2a2550), hover ? 0.9f : 0.6f));
        Text(label, row.x + 18, row.y + 13, 17, Hex(0xe6e1ff), F_MEDIUM);
        TextRight(value, row.x + row.width - 18, row.y + 13, 17, Hex(0xb9b0ff), F_BOLD);
        if (hover) WantHand();
        return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }

    // ------------------------------------------------------------- story --
    void StartStory(const Panel* p, int n, State after, int nextLand = 0) {
        storyLand = nextLand;
        panels = p;
        panelCount = n;
        panelIdx = 0;
        panelT = 0;
        afterStory = after;
        state = STORY;
        snd::PlayMusic("story");
    }

    void Story(float dt) {
        panelT += dt;
        Rectangle scr = ScreenRect();
        DrawRectangleRec(scr, Hex(0x0b0a14));
        const Panel& p = panels[panelIdx];
        // the picture
        Rectangle art = {W / 2 - 420, 70, 840, 420};
        DrawRectangleRounded(art, 0.06f, 10, Hex(0x15122b));
        BeginClip(art);
        DrawRectangleGradientV((int)art.x, (int)art.y, (int)art.width, (int)art.height, Hex(0x1b1640), Hex(0x3a2a6b));
        // behind the picture, the painting of where the story is: the land just
        // beaten on the first panel, then the next one (the Heap for the SEGFAULT)
        int where;
        if (panels == kIntro) where = p.art == 2 ? 5 : 0;
        else if (panels == kAfterChapter5) where = panelIdx == 0 ? 4 : 5;
        else if (panels == kAfterChapter6) where = panelIdx < 2 ? 5 : 0;
        else where = panelIdx == 0 ? storyLand - 1 : storyLand;
        fp::DrawBackdropIn(art, where, t);
        fp::SetLandLight(where);
        Vector2 mid = {art.x + art.width / 2, art.y + art.height};
        switch (p.art) {
            case 0: fp::DrawCastle({art.x + 120, art.y + 40, art.width - 240, art.height - 40}, t); break;
            case 1: fp::DrawAda({mid.x, mid.y - 30}, 1.6f, t); break;
            case 2:
                fp::DrawSegfault({mid.x + 80, mid.y - 120}, 0.62f, t);
                fp::DrawAda({mid.x - 230, mid.y - 30}, 0.9f, t);
                break;
            case 3: fp::DrawHero({mid.x, mid.y - 30}, 2.0f, 1, 0, -1, t, false); break;
            case 4: fp::DrawSerpent({mid.x, mid.y - 200}, 140, t * 0.2f, 0, 1); break;
            case 5:   // the canyon: the Golem and two imps
                fp::DrawGolem({mid.x + 120, mid.y - 24}, 0.9f, t, 0.15f + 0.15f * std::sin(t * 1.2f), 0, 1, 0);
                fp::DrawImp({mid.x - 230, mid.y - 30 - std::fabs(std::sin(t * 3)) * 40}, 1.2f, 1, std::fabs(std::sin(t * 3)), 0, 1, 4, t);
                fp::DrawImp({mid.x - 110, mid.y - 30 - std::fabs(std::sin(t * 3 + 1.5f)) * 40}, 1.2f, 1, std::fabs(std::sin(t * 3 + 1.5f)), 0, 1, -1, t);
                break;
            case 6: fp::DrawGolem({mid.x, mid.y - 24}, 0.9f, t, 0, 0, 1, 0.9f); break;
            case 7:   // the peaks: the Wyvern and two ghosts
                fp::DrawWyvern({mid.x + 150, mid.y - 230 + std::sin(t * 1.5f) * 12}, 1.0f, -1, 0, std::fmod(t * 1.4f, 1.0f), 0, 1, t);
                fp::DrawGhost({mid.x - 250, mid.y - 70}, 1.2f, 1, 0, 1, 1, t);
                fp::DrawGhost({mid.x - 110, mid.y - 50}, 1.2f, 1, 0, 1, 0.5f + 0.5f * std::sin(t * 1.3f), t + 1);
                break;
            case 8: fp::DrawWyvern({mid.x, mid.y - 110}, 1.0f, -1, 70, 0.25f, 0, 0.75f, t); break;   // fallen in the snow
            case 9:   // the forest: the Leak Lord and two of its slimes
                fp::DrawLeakLord({mid.x + 150, mid.y - 20}, 0.62f, t, 0.2f + 0.2f * std::sin(t * 1.7f), 0, 1, 0);
                fp::DrawSlime({mid.x - 250, mid.y - 30}, 1.3f, 1, 0, 1.0f, 0, 1, 32, t);
                fp::DrawSlime({mid.x - 110, mid.y - 30}, 1.0f, 1, 0, 1.0f, 0, 1, 16, t + 1);
                break;
            case 10: fp::DrawLeakLord({mid.x, mid.y - 20}, 0.62f, t, 0, 0, 0.85f, 0.8f); break;   // melted
            case 11: {   // the city: the Tangled List, and a line of nodes
                fp::DrawTangledList({mid.x + 170, mid.y - 16}, 0.52f, t, 0.2f + 0.2f * std::sin(t * 1.3f), 0, 1, 0);
                const float xs[3] = {mid.x - 150, mid.x - 250, mid.x - 350};
                for (int i = 0; i < 3; i++) {
                    Vector2 f = {xs[i], mid.y - 28};
                    if (i < 2) fp::DrawLink(fp::NodeNextDot(f, 1.25f, -1), fp::NodeInlet({xs[i + 1], mid.y - 28}, 1.25f, -1), 1, t);
                    fp::DrawNode(f, 1.25f, -1, t * 6 + i, 0, 1, (i * 37 + 12) % 90, i == 0, i == 2, t);
                }
                break;
            }
            case 12: fp::DrawTangledList({mid.x, mid.y - 16}, 0.52f, t, 0, 0, 0.85f, 0.85f); break;   // undone
            case 13:   // Ada, free, a glow of the Kernel around her
                fp::DrawShadow({mid.x, mid.y - 30}, 46, 0);
                fp::DrawAda({mid.x, mid.y - 30}, 1.6f, t);
                break;
            case 14:   // the Debugger and Ada, together
                fp::DrawShadow({mid.x - 90, mid.y - 30}, 46, 0);
                fp::DrawShadow({mid.x + 90, mid.y - 30}, 40, 0);
                fp::DrawHero({mid.x - 90, mid.y - 30}, 1.6f, 1, 0, -1, t, false);
                fp::DrawAda({mid.x + 90, mid.y - 30}, 1.5f, t);
                break;
            case 15: fp::DrawSegfault({mid.x, mid.y - 130}, 0.6f, t, 0, 0.4f + 0.25f * std::sin(t * 7)); break;   // fading out
        }
        EndClip();
        DrawRectangleRoundedLinesEx(art, 0.06f, 10, 2, Hex(0x4b4475));
        // the words, typed out
        int shown = std::min((int)std::string(p.text).size(), (int)(panelT * 45));
        std::string text = std::string(p.text).substr(0, shown);
        float y = 530;
        for (const std::string& line : Wrap(text, 22, F_MEDIUM, 820)) {
            TextCenter(line, {0, y, W, 30}, 22, Hex(0xe6e1ff), F_MEDIUM);
            y += 34;
        }
        bool done = shown >= (int)std::string(p.text).size();
        // dots for the panels
        for (int i = 0; i < panelCount; i++)
            DrawCircleV({W / 2 - (panelCount - 1) * 11.0f + i * 22, H - 70}, 5, i == panelIdx ? Hex(0xb9b0ff) : Hex(0x3b3470));
        TextCenter(done ? "Enter / click to continue   -   Esc to skip" : "", {0, H - 46, W, 20}, 13, Hex(0x8f88b8), F_MEDIUM);
        bool next = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (next && !done) panelT = 1000;   // show the whole text first
        else if (next) {
            if (++panelIdx >= panelCount) EndStory();
            else panelT = 0;
        }
        if (IsKeyPressed(KEY_ESCAPE)) EndStory();
    }

    void EndStory() {
        if (afterStory == CHAPTER) StartChapter(storyLand);
        else {
            state = afterStory;
            if (state == MENU) { menuSel = 0; snd::PlayMusic("title"); }
            if (state == CREDITS) { creditsT = 0; snd::PlayMusic("title"); }
        }
    }

    // ------------------------------------------------------- chapter card --
    void StartChapter(int which) {
        land = std::clamp(which, 0, kLandCount - 1);
        snd::PlayMusic(kLands[land].music);
        state = CHAPTER;
        chapterT = 0;
        ResetLevel();
    }

    void Chapter(float dt) {
        chapterT += dt;
        Draw();
        float a = chapterT < 2.2f ? 1 : 1 - Clamp01((chapterT - 2.2f) / 0.6f);
        Shade(0.75f * a);
        TextCenter(TextFormat("CHAPTER %d", land + 1), {0, 280, W, 30}, 20, Fade(Hex(0xb9b0ff), a), F_BOLD);
        TextCenter(kLands[land].name, {0, 312, W, 70}, 60, Fade(WHITE, a), F_HEAVY);
        TextCenter(kLands[land].topic, {0, 392, W, 26}, 18, Fade(Hex(0xd8d3f5), a), F_MEDIUM);
        if (chapterT > 2.8f || ((IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) && chapterT > 0.3f)) state = PLAY;
    }

    // ------------------------------------------------------------ the level --
    void ResetLevel() {
        hero = Hero();
        hero.hearts = set.Hearts();
        goblins.clear();
        const float xs[] = {1000, 1500, 2050, 2550, 3100, 3650};
        const int oob[] = {4, 5, -1, 8, 10, 16};   // indices past the end of an int a[4] (or before it)
        for (float x : xs) {
            Goblin g;
            g.x = g.home = x;
            g.hp = g.maxHp = GetRandomValue(2, 3);
            g.index = oob[GetRandomValue(0, 5)];
            g.hopCd = Rand01();
            goblins.push_back(g);
        }
        if (land == 4) {   // nodes: three lines of three, each walking behind the one in front
            goblins.clear();
            const float starts[3] = {1150, 2350, 3550};
            for (int grp = 0; grp < 3; grp++)
                for (int i = 0; i < 3; i++) {
                    Goblin g;
                    g.x = g.home = starts[grp] + i * 82.0f;
                    g.hp = g.maxHp = 2;
                    g.group = grp;
                    g.order = i;
                    g.index = GetRandomValue(1, 99);
                    goblins.push_back(g);
                }
        }
        if (land == 5)   // bats: up high, diving at different times
            for (Goblin& g : goblins) { g.hy = 150; g.index = 0; g.hopCd = 1.0f + Rand01() * 2; }
        beams.clear();
        walls.clear();
        sfAct = 0;
        sfCd = 2.4f;
        tiles.clear();
        tlAct = 0;
        tlCd = 2.4f;
        tlPose = 0;
        tlP = -1;
        if (land == 3)   // slimes: small to start with; they grow as they leak
            for (Goblin& g : goblins) { g.vy = 1; g.index = 16; g.hopCd = 1.0f + Rand01() * 2; }
        puddles.clear();
        blobs.clear();
        parts.clear();
        combo = 0;
        comboT = hitStop = hurtFlash = look = 0;
        lordAct = 0;
        lordCd = 2.4f;
        lordMouth = 0;
        if (land == 2)   // ghosts: solid to start with, turning NULL at different times
            for (Goblin& g : goblins) { g.index = 1; g.vy = 1; g.hy = 40; g.hopCd = 1.5f + Rand01() * 2.5f; }
        rings.clear();
        rocks.clear();
        bolts.clear();
        wyAct = 0;
        wyCd = 2.5f;
        wyTilt = 0;
        bossY = 250;
        golemPose = 0;
        golemCd = 2.5f;
        golemAct = 0;
        popups.clear();
        arena = bossAwake = false;
        bossHp = 1;
        bossDying = -1;
        bossHurt = 0;
        bestPassed = 0;
        termMsg.clear();
        cam = 0;
        // questions about this land's topic (the Heap: about everything), shuffled; each one once before any repeats
        const Land& ld = kLands[land];
        questionPool.clear();
        for (const Question& qq : Questions())
            if (land == 5 || qq.section == ld.a || qq.section == ld.b) questionPool.push_back(qq.id);
        Shuffle(questionPool);
        poolNext = 0;
        // the boss's problem: a medium one on the topic (C, so any compiler can run it)
        std::vector<const Problem*> pick;
        if (land == 5)   // the final boss: one of the hardest problems, from any section
            for (const Problem& p : Problems())
                if (p.difficulty == 2 && !p.cppOnly) pick.push_back(&p);
        for (const Problem& p : Problems())
            if (land != 5 && (p.section == ld.a || p.section == ld.b) && p.difficulty == 1 && !p.cppOnly) pick.push_back(&p);
        if (pick.empty())
            for (const Problem& p : Problems())
                if ((p.section == ld.a || p.section == ld.b) && p.difficulty == 1) pick.push_back(&p);
        bossProblem = pick.empty() ? nullptr : pick[GetRandomValue(0, (int)pick.size() - 1)];
        if (bossProblem) ws.Open(bossProblem, "", true);
    }

    template <class T> static void Shuffle(std::vector<T>& v) {
        for (int i = (int)v.size() - 1; i > 0; i--) std::swap(v[i], v[GetRandomValue(0, i)]);
    }

    void Pop(const std::string& s, float x, float y, Color c) { popups.push_back({s, x, y, 0, c}); }

    // ---- lines of nodes: the one in front (-1: this one is the head) and the one behind (-1: the tail)
    int Ahead(const Goblin& g) const {
        int best = -1;
        for (size_t j = 0; j < goblins.size(); j++) {
            const Goblin& o = goblins[j];
            if (&o == &g || !o.alive || o.dying >= 0 || o.group != g.group || o.order >= g.order) continue;
            if (best < 0 || o.order > goblins[best].order) best = (int)j;
        }
        return best;
    }
    int Behind(const Goblin& g) const {
        int best = -1;
        for (size_t j = 0; j < goblins.size(); j++) {
            const Goblin& o = goblins[j];
            if (&o == &g || !o.alive || o.dying >= 0 || o.group != g.group || o.order <= g.order) continue;
            if (best < 0 || o.order < goblins[best].order) best = (int)j;
        }
        return best;
    }

    // ---- particles and shake
    void Burst(float x, float y, int n, Color c, float speed, int kind = 0, float grav = 900, float size = 4) {
        for (int i = 0; i < n && parts.size() < 700; i++) {
            float a = Rand01() * 2 * PI, sp = speed * (0.35f + 0.65f * Rand01());
            float life = 0.35f + 0.45f * Rand01();
            parts.push_back({x, y, std::cos(a) * sp, std::sin(a) * sp - speed * 0.3f, life, life, size * (0.6f + 0.8f * Rand01()), grav, c, kind});
        }
    }
    // Dust kicked up at the feet (dir: which way it blows; 0 both ways).
    void Dust(float x, int dir) {
        const unsigned col[] = {0xe8f0d8, 0xf0dcc0, 0xf4f8ff, 0xb8d8b0};
        for (int i = 0; i < 9 && parts.size() < 700; i++) {
            float side = dir != 0 ? (float)dir : (i % 2 ? 1.0f : -1.0f);
            float sp = 60 + 120 * Rand01();
            parts.push_back({x + side * 10, kGround - 4, side * sp, -30 - 50 * Rand01(), 0.45f, 0.45f, 5 + 4 * Rand01(), -40,
                             Fade(Hex(col[std::clamp(land, 0, 3)]), 0.8f), 0});
        }
    }
    void Shake(float amount) { shake = std::max(shake, amount); }
    void Hit(float stop, float shakeBy) { hitStop = std::max(hitStop, stop); Shake(shakeBy); }

    void HurtHero(int fromDir) {
        if (hero.hurt > 0 || hero.invuln > 0) return;
        hero.hearts--;
        hero.hurt = 1.0f;
        hero.vy = -420;
        hero.grounded = false;
        hero.x += fromDir * 60;
        hero.jumping = false;
        Hit(0.07f, 0.6f);
        hurtFlash = 0.5f;
        combo = 0;
        Burst(hero.x, hero.y - 80, 14, Hex(0xff5c7a), 380, 1, 900, 4);
        Fx(snd::FX_HURT);
        Pop("-1", hero.x, hero.y - 205, Hex(0xff5c7a));
        if (hero.hearts <= 0) { state = DEAD; deadT = 0; snd::PlayMusic("gameover"); }
    }

    void Update(float dt) {
        if (IsKeyPressed(KEY_ESCAPE)) { state = PAUSE; menuSel = 0; return; }
        Rectangle scr = ScreenRect();
        const float L = scr.x, vw = scr.width;
        // ---- the hero
        // run, jump (coyote time + a jump buffer; let go early for a shorter hop), dash
        float move = (Down(ACT_RIGHT) ? 1.0f : 0) - (Down(ACT_LEFT) ? 1.0f : 0);
        if (move != 0 && hero.dash <= 0) hero.face = move > 0 ? 1 : -1;
        hero.dashCd = std::max(0.0f, hero.dashCd - dt);
        hero.invuln = std::max(0.0f, hero.invuln - dt);
        if (Pressed(ACT_DASH) && hero.dashCd <= 0) {
            hero.dash = 0.16f;
            hero.dashCd = 0.6f;
            hero.invuln = 0.24f;   // dashing through an attack is safe
            hero.jumping = false;
            Fx(snd::FX_DASH, 0.8f);
            if (hero.grounded) Dust(hero.x, -hero.face);
        }
        if (hero.dash > 0) {
            hero.dash -= dt;
            hero.x += hero.face * 1150 * dt;
            hero.vy = 0;
            for (int i = 0; i < 2; i++)   // speed lines behind it
                parts.push_back({hero.x - hero.face * (20 + 30 * Rand01()), hero.y - 30 - 110 * Rand01(), -hero.face * 300.0f, 0, 0.22f, 0.22f, 3, 0,
                                 Fade(Hex(0x9fe9ff), 0.8f), 2});
        } else hero.x += move * 340 * dt;
        hero.walk = move != 0 && hero.grounded ? hero.walk + dt * 11 : 0;
        hero.coyote = hero.grounded ? 0.10f : hero.coyote - dt;
        hero.jumpBuf = Pressed(ACT_JUMP) ? 0.13f : hero.jumpBuf - dt;
        if (hero.jumpBuf > 0 && hero.coyote > 0 && hero.dash <= 0) {
            hero.vy = -820;
            hero.grounded = false;
            hero.jumping = true;
            hero.coyote = hero.jumpBuf = 0;
            hero.squash = 0.26f;
            Fx(snd::FX_JUMP, 0.7f);
            Dust(hero.x, 0);
        }
        if (hero.jumping && !Down(ACT_JUMP) && hero.vy < -330) hero.vy = -330;   // let go early: a shorter hop
        if (hero.vy >= 0) hero.jumping = false;
        if (hero.dash <= 0) hero.vy += 2300 * dt;
        float falling = hero.vy;
        hero.y += hero.vy * dt;
        if (hero.y >= kGround) {
            if (!hero.grounded && falling > 300) { hero.squash = -std::min(0.3f, falling / 3200); Dust(hero.x, 0); }
            hero.y = kGround;
            hero.vy = 0;
            hero.grounded = true;
        }
        hero.squash *= 1 - std::min(1.0f, dt * 12);
        hero.hurt = std::max(0.0f, hero.hurt - dt);
        hero.attackCd = std::max(0.0f, hero.attackCd - dt);
        if (hero.attack >= 0) { hero.attack += dt / 0.22f; if (hero.attack >= 1) hero.attack = -1; }
        // the walls: the start of the level, and the arena once the boss wakes
        float lo = 60, hi = kLevelLen - 60;
        if (arena) { lo = arenaCam + L + 40; hi = arenaCam + L + vw - 40; }
        hero.x = std::clamp(hero.x, lo, hi);

        // ---- attack: the nearest goblin in front
        if (Pressed(ACT_ATTACK) && hero.attackCd <= 0) {
            hero.attack = 0;
            hero.attackCd = 0.35f;
            Fx(snd::FX_SWING);
            for (size_t i = 0; i < goblins.size(); i++) {
                Goblin& g = goblins[i];
                float dx = (g.x - hero.x) * hero.face;
                if (!g.alive || g.dying >= 0 || dx < -20 || dx > 130 || std::fabs((hero.y - kGround) + g.hy) > 70) continue;
                if (land == 2 && g.vy < 0.5f) continue;   // NULL: the sword goes straight through
                Strike((int)i);
                break;
            }
        }

        // ---- goblins: patrol, chase when close, hurt on touch
        for (Goblin& g : goblins) {
            if (!g.alive) continue;
            g.hurt = std::max(0.0f, g.hurt - dt * 3);
            g.touchCd = std::max(0.0f, g.touchCd - dt);
            if (g.dying >= 0) { g.dying += dt; g.hy = std::max(0.0f, g.hy - 300 * dt); if (g.dying > 0.6f) g.alive = false; continue; }
            float dx = hero.x - g.x;
            if (land == 5) {   // a dangling bat: it flies above you and now and then dives (hit it low, or jump)
                g.walk += dt * (g.index ? 22.0f : 12.0f);
                bool chase = std::fabs(dx) < 560;
                if (g.index == 0) {
                    float want = 150 + 22 * std::sin(t * 2 + g.home * 0.01f);
                    g.hy += (want - g.hy) * std::min(1.0f, dt * 3);
                    if (chase) { g.face = dx > 0 ? 1 : -1; if (std::fabs(dx) > 120) g.x += g.face * 110 * dt; }
                    else {
                        if (g.x > g.home + 140) g.face = -1;
                        if (g.x < g.home - 140) g.face = 1;
                        g.x += g.face * 60 * dt;
                    }
                    g.hopCd -= dt;
                    if (chase && g.hopCd <= 0 && std::fabs(dx) < 380) { g.index = 1; g.vy = 0; Fx(snd::FX_BAT, 0.7f); }
                } else {   // the dive: down to the ground and up again, flying on
                    g.vy += dt;
                    float u = std::min(1.0f, g.vy / 1.1f);
                    g.hy = 150 - 132 * std::sin(u * PI);
                    g.x += g.face * 260 * dt;
                    if (u >= 1) { g.index = 0; g.hopCd = 1.8f + Rand01() * 1.4f; }
                }
                float by = kGround - g.hy - 40, hy2 = hero.y - 70;
                if (std::fabs(dx) < 46 && std::fabs(by - hy2) < 75 && g.touchCd <= 0 && hero.hurt <= 0) {
                    g.touchCd = 1.0f;
                    HurtHero(dx > 0 ? 1 : -1);
                    if (state != PLAY) return;
                }
                continue;
            }
            if (land == 4) {   // a lost node: the head of a line chases you, the rest follow the one in front
                int lead = Ahead(g);
                float speed = 0;
                if (lead < 0) {
                    if (std::fabs(dx) < 480) { g.face = dx > 0 ? 1 : -1; speed = std::fabs(dx) > 40 ? 120.0f : 0; }
                    else {
                        if (g.x > g.home + 120) g.face = -1;
                        if (g.x < g.home - 120) g.face = 1;
                        speed = 50;
                    }
                    g.x += g.face * speed * dt;
                } else {
                    const Goblin& o = goblins[lead];
                    float d = (o.x - o.face * 82) - g.x;   // its place, just behind the one in front
                    float sp = std::clamp(d * 4, -220.0f, 220.0f);
                    g.x += sp * dt;
                    g.face = o.face;
                    speed = std::fabs(sp);
                }
                g.walk = speed > 5 ? g.walk + dt * 9 : 0;
                if (std::fabs(dx) < 44 && std::fabs(hero.y - kGround) < 70 && g.touchCd <= 0 && hero.hurt <= 0) {
                    g.touchCd = 1.0f;
                    HurtHero(dx > 0 ? 1 : -1);
                    if (state != PLAY) return;
                }
                continue;
            }
            if (land == 3) {   // a leak slime: it oozes at you, leaking puddles and growing as it goes
                bool chase = std::fabs(dx) < 520;
                g.walk += dt;
                if (chase) g.face = dx > 0 ? 1 : -1;
                else {
                    if (g.x > g.home + 120) g.face = -1;
                    if (g.x < g.home - 120) g.face = 1;
                }
                if (!chase || std::fabs(dx) > 34) g.x += g.face * (chase ? 90.0f : 45.0f) * (g.minion ? 1.7f : 1.0f) * dt;
                if (!g.minion) {
                    g.hopCd -= dt;
                    if (g.hopCd <= 0) {
                        g.hopCd = 2.4f + Rand01();
                        if (chase && puddles.size() < 14) {   // a leak: a puddle behind it, and it grows
                            puddles.push_back({g.x - g.face * 40 * g.vy, 1.0f});
                            g.vy = std::min(1.45f, g.vy + 0.09f);
                            g.index = std::min(1024, g.index * 2);
                            Fx(snd::FX_DRIP, 0.5f);
                        }
                    }
                }
                if (std::fabs(dx) < 40 * g.vy && hero.y > kGround - 80 * g.vy && g.touchCd <= 0 && hero.hurt <= 0) {
                    g.touchCd = 1.0f;
                    HurtHero(dx > 0 ? 1 : -1);
                    if (state != PLAY) return;
                }
                continue;
            }
            if (land == 2) {   // a NULL ghost: it floats at you, and every few seconds turns NULL
                g.hopCd -= dt;
                if (g.hopCd <= 0) {
                    if (g.index == 1) { g.index = 0; g.hopCd = 1.4f; if (std::fabs(dx) < 700) Fx(snd::FX_GHOST, 0.5f); }
                    else { g.index = 1; g.hopCd = 2.2f + Rand01() * 1.8f; }
                }
                g.vy += ((float)g.index - g.vy) * std::min(1.0f, dt * 6);   // fades in and out
                g.hy = 40 + 22 * std::sin(t * 1.7f + g.home * 0.01f);
                bool chase = std::fabs(dx) < 520;
                if (chase) { g.face = dx > 0 ? 1 : -1; if (std::fabs(dx) > 30) g.x += g.face * 115 * dt; }
                else {
                    if (g.x > g.home + 120) g.face = -1;
                    if (g.x < g.home - 120) g.face = 1;
                    g.x += g.face * 55 * dt;
                }
                if (g.vy > 0.5f && std::fabs(dx) < 46 && std::fabs((hero.y - kGround) + g.hy) < 80 && g.touchCd <= 0 && hero.hurt <= 0) {
                    g.touchCd = 1.0f;
                    HurtHero(dx > 0 ? 1 : -1);
                    if (state != PLAY) return;
                }
                continue;
            }
            if (land == 1) {   // an imp: it hops at you
                bool chase = std::fabs(dx) < 520;
                if (g.hy > 0 || g.vy < 0) {   // in the air
                    g.vy += 1900 * dt;
                    g.hy -= g.vy * dt;
                    float sp = chase ? (std::fabs(dx) > 30 ? 230.0f : 40.0f) : 95.0f;
                    g.x += g.face * sp * dt;
                    if (g.hy <= 0) { g.hy = 0; g.vy = 0; g.hopCd = chase ? 0.35f + Rand01() * 0.45f : 0.6f + Rand01() * 0.6f; }
                } else {
                    if (chase) g.face = dx > 0 ? 1 : -1;
                    else {   // little hops around home
                        if (g.x > g.home + 120) g.face = -1;
                        if (g.x < g.home - 120) g.face = 1;
                    }
                    g.hopCd -= dt;
                    if (g.hopCd <= 0) {
                        g.vy = chase ? -(560 + Rand01() * 200) : -380;
                        g.hy = 0.01f;
                        if (std::fabs(dx) < 700) Fx(snd::FX_HOP, chase ? 0.6f : 0.3f);
                    }
                }
                if (std::fabs(dx) < 46 && std::fabs((hero.y - kGround) + g.hy) < 80 && g.touchCd <= 0 && hero.hurt <= 0) {
                    g.touchCd = 1.0f;
                    HurtHero(dx > 0 ? 1 : -1);
                    if (state != PLAY) return;
                }
                continue;
            }
            float speed = 0;
            if (std::fabs(dx) < 420) { g.face = dx > 0 ? 1 : -1; speed = std::fabs(dx) > 36 ? 130.0f : 0; }   // runs at you
            else {   // patrol around home
                if (g.x > g.home + 120) g.face = -1;
                if (g.x < g.home - 120) g.face = 1;
                speed = 60;
            }
            g.x += g.face * speed * dt;
            g.walk = speed > 0 ? g.walk + dt * 9 : 0;
            if (std::fabs(dx) < 46 && std::fabs(hero.y - kGround) < 70 && g.touchCd <= 0 && hero.hurt <= 0) {
                g.touchCd = 1.0f;
                HurtHero(dx > 0 ? 1 : -1);
                if (state != PLAY) return;
            }
        }

        // ---- the arena: once every goblin is gone and the hero walks far enough
        if (!arena && hero.x > kLevelLen - vw * 0.55f) {
            arena = true;
            arenaCam = kLevelLen - vw - L;   // the last screen of the level
            termX = arenaCam + L + 150;
            bossX = arenaCam + L + vw * (land == 0 ? 0.68f : land == 1 ? 0.72f : land == 2 ? 0.80f : land == 3 ? 0.78f : land == 4 ? 0.76f : 0.80f);
            sfCd = 2.0f;
            sfAct = 0;
            tlCd = 2.0f;
            tlAct = 0;
            lordCd = 2.0f;
            lordAct = 0;
            bossY = 220;
            wyCd = 2.0f;
            wyAct = 0;
            bossT = 0;
            ringCd = 2.5f;
            golemCd = 2.2f;
            termCd = 0;
            Fx(land == 0 ? snd::FX_ROAR : land == 1 ? snd::FX_GOLEM_WAKE : land == 2 ? snd::FX_SCREECH : land == 3 ? snd::FX_BURP : land == 4 ? snd::FX_CREAK : snd::FX_ROAR);
            snd::PlayMusic(kLands[land].bossMusic);
            shake = 0.6f;
            Pop(std::string("The ") + kLands[land].boss + " awakens!", hero.x, hero.y - 220, land == 0 ? Hex(0x0f7f73) : land == 1 ? Hex(0xa0441c) : land == 2 ? Hex(0x2a5aa0) : land == 3 ? Hex(0x2f8a4c) : land == 4 ? Hex(0x1d6c7c) : Hex(0x9a1030));
            bossAwake = true;
        }
        if (bossAwake && bossDying < 0) {
            bossT += dt;
            bossHurt = std::max(0.0f, bossHurt - dt * 2);
            if (land == 0) {
                // it drifts back and forth over the right of the arena
                float span = vw * 0.22f;
                bossX = arenaCam + L + vw * 0.66f + std::sin(bossT * 0.7f) * span;
                // spits loop rings that roll along the ground: jump over them
                ringCd -= dt;
                if (ringCd <= 0) {
                    rings.push_back({bossX - 120, -(300.0f + GetRandomValue(0, 140)), 6});
                    Fx(snd::FX_SPIT, 0.8f);
                    ringCd = 1.6f + Rand01() * 1.2f;
                }
                // touching the coil hurts
                Vector2 bc = BossCenter();
                float ddx = hero.x - bc.x, ddy = (hero.y - 70) - bc.y;
                if (ddx * ddx + ddy * ddy < 170.0f * 170.0f) HurtHero(ddx > 0 ? 1 : -1);
            } else if (land == 1) UpdateGolem(dt, L, vw);
            else if (land == 2) UpdateWyvern(dt, L, vw);
            else if (land == 3) UpdateLeakLord(dt, L, vw);
            else if (land == 4) UpdateTangle(dt, L, vw);
            else UpdateSegfault(dt, L, vw);
            if (state != PLAY) return;
            termCd = std::max(0.0f, termCd - dt);
            bool near = std::fabs(hero.x - termX) < 90;
            if (near && termCd <= 0 && Pressed(ACT_USE) && bossProblem) { state = TERMINAL; wsRunning = false; termMsg.clear(); Fx(snd::FX_TERMINAL); return; }
        }
        if (bossDying >= 0) {
            if (bossDying == 0) {   // the boss is beaten: its slimes, its blobs and its list go with it
                for (Goblin& g : goblins) if (g.minion && g.alive && g.dying < 0) g.dying = 0;
                blobs.clear();
                tiles.clear();
                beams.clear();
                walls.clear();
                Burst(bossX, kGround - 150, 60, Hex(kLands[land].bossColor), 700, 1, 900, 7);
                Hit(0.18f, 1.0f);
            }
            bossDying += dt;
            if (bossDying > 2.4f) { Win(); return; }
        }
        // the Golem's blocks: a mark, then the fall; landing on the hero hurts
        for (Rock& r : rocks) {
            if (r.landed >= 0) { r.landed += dt; continue; }
            if (r.warn > 0) { r.warn -= dt; if (r.warn <= 0) Fx(snd::FX_ROCK_FALL, 0.6f); continue; }
            r.vy += 2600 * dt;
            r.y += r.vy * dt;
            if (r.y >= kGround - 37) {
                r.y = kGround - 37;
                r.landed = 0;
                shake = std::max(shake, 0.12f);
                if (std::fabs(hero.x - r.x) < 58 && hero.y > kGround - 190) { HurtHero(hero.x > r.x ? 1 : -1); if (state != PLAY) return; }
            }
        }
        rocks.erase(std::remove_if(rocks.begin(), rocks.end(), [](const Rock& r) { return r.landed > 0.5f; }), rocks.end());
        // the Wyvern's bolts
        for (Bolt& b : bolts) {
            b.p.x += b.v.x * dt;
            b.p.y += b.v.y * dt;
            float bx = b.p.x - hero.x, by = b.p.y - (hero.y - 70);
            if (bx * bx + by * by < 46.0f * 46.0f) { b.p.y = 99999; HurtHero(b.v.x > 0 ? 1 : -1); if (state != PLAY) return; }
        }
        // the Leak Lord's blobs: they arc down and leave a puddle where they land
        for (Bolt& b : blobs) {
            b.v.y += 1400 * dt;
            b.p.x += b.v.x * dt;
            b.p.y += b.v.y * dt;
            float bx = b.p.x - hero.x, by = b.p.y - (hero.y - 70);
            if (bx * bx + by * by < 40.0f * 40.0f) { b.p.y = 99999; HurtHero(b.v.x > 0 ? 1 : -1); if (state != PLAY) return; continue; }
            if (b.p.y >= kGround - 8) {
                if (puddles.size() < 16) puddles.push_back({b.p.x, 1.0f});
                Burst(b.p.x, kGround - 6, 10, Hex(0x7be36b), 260, 0, 900, 5);
                Fx(snd::FX_DRIP, 0.5f);
                b.p.y = 99999;
            }
        }
        blobs.erase(std::remove_if(blobs.begin(), blobs.end(), [](const Bolt& b) { return b.p.y > 9999; }), blobs.end());
        // the SEGFAULT's crash columns: a mark, then the beam (standing in it hurts)
        for (Beam& b : beams) {
            if (b.warn > 0) {
                b.warn -= dt;
                if (b.warn <= 0) { b.life = 1; Fx(snd::FX_BEAM, 0.7f); Shake(0.22f); }
                continue;
            }
            b.life -= dt * 2.8f;
            if (b.life > 0.5f && std::fabs(hero.x - b.x) < 40) { HurtHero(hero.x > b.x ? 1 : -1); if (state != PLAY) return; }
        }
        beams.erase(std::remove_if(beams.begin(), beams.end(), [](const Beam& b) { return b.warn <= 0 && b.life <= 0; }), beams.end());
        // blue screens sweep the arena: only a dash (or a moment of being hurt) gets through
        for (float& w : walls) {
            w -= 540 * dt;
            if (std::fabs(w - hero.x) < 34) { HurtHero(-1); if (state != PLAY) return; }
        }
        walls.erase(std::remove_if(walls.begin(), walls.end(), [&](float w) { return w < arenaCam + L - 200; }), walls.end());
        // puddles dry up after a while; stepping in one hurts
        for (Puddle& p : puddles) {
            p.life -= dt / 5;
            if (p.life > 0.15f && std::fabs(p.x - hero.x) < 34 && hero.y >= kGround - 4) { HurtHero(hero.x > p.x ? 1 : -1); if (state != PLAY) return; }
        }
        puddles.erase(std::remove_if(puddles.begin(), puddles.end(), [](const Puddle& p) { return p.life <= 0; }), puddles.end());
        bolts.erase(std::remove_if(bolts.begin(), bolts.end(), [&](const Bolt& b) {
            return b.p.y > kGround + 10 || b.p.x < arenaCam + L - 120 || b.p.x > arenaCam + L + vw + 120 || b.p.y < -120; }), bolts.end());
        for (Ring& r : rings) {
            r.x += r.vx * dt;
            r.life -= dt;
            if (std::fabs(r.x - hero.x) < 38 && hero.y > kGround - 50 && r.life > 0) { r.life = 0; HurtHero(r.vx > 0 ? 1 : -1); if (state != PLAY) return; }
        }
        rings.erase(std::remove_if(rings.begin(), rings.end(), [&](const Ring& r) { return r.life <= 0 || r.x < arenaCam + L - 100; }), rings.end());

        // ---- camera
        look += (hero.face * 90 - look) * std::min(1.0f, dt * 2.0f);   // looks ahead, where the hero is going
        float target = hero.x - (L + vw * 0.4f) + look;
        target = std::clamp(target, -L, kLevelLen - vw - L);
        if (arena) target = arenaCam;
        cam += (target - cam) * std::min(1.0f, dt * 6);
        shake = std::max(0.0f, shake - dt * 1.6f);
        hurtFlash = std::max(0.0f, hurtFlash - dt * 1.5f);
        comboPop = std::max(0.0f, comboPop - dt * 4);
        if (comboT > 0) { comboT -= dt; if (comboT <= 0) combo = 0; }
        for (Particle& p : parts) {
            p.vy += p.grav * dt;
            p.vx *= 1 - std::min(1.0f, dt * 2.5f);
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.life -= dt;
        }
        parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());
        for (Popup& p : popups) p.age += dt;
        popups.erase(std::remove_if(popups.begin(), popups.end(), [](const Popup& p) { return p.age > 1.4f; }), popups.end());
    }

    Vector2 BossCenter() const { return {bossX, 330 + std::sin(bossT * 1.3f) * 30}; }

    // The SEGFAULT: it stands at the far end of the Heap and brings down columns
    // of crash where you stand (move), throws its broken pointer along the
    // ground (jump, or dash through it), or sends a blue screen across the
    // whole arena (only a dash gets through). Once half its tests pass, it rages:
    // faster, and one more column.
    void UpdateSegfault(float dt, float L, float vw) {
        bossX = arenaCam + L + vw * 0.80f + std::sin(bossT * 0.5f) * vw * 0.02f;
        const bool rage = bossHp < 0.5f;
        const float pace = rage ? 0.7f : 1.0f;
        if (sfAct == 0) {
            sfCd -= dt;
            if (sfCd <= 0) {
                float r = Rand01();
                sfAct = r < 0.4f ? 1 : r < 0.75f ? 2 : 3;
                sfT = 0;
                if (sfAct == 1) {   // columns: one where you stand, the rest apart from it
                    float lo = arenaCam + L + 110, hi = bossX - 210;
                    std::vector<float> xs = {std::clamp(hero.x, lo, hi)};
                    for (int tries = 0; tries < 30 && (int)xs.size() < (rage ? 4 : 3); tries++) {
                        float x = lo + Rand01() * (hi - lo);
                        bool apart = true;
                        for (float o : xs) apart &= std::fabs(o - x) > 130;
                        if (apart) xs.push_back(x);
                    }
                    for (size_t i = 0; i < xs.size(); i++) {
                        float warn = (0.9f + 0.15f * i) * (rage ? 0.85f : 1.0f);
                        beams.push_back({xs[i], warn, warn, 0});
                    }
                }
                if (sfAct == 3) {
                    Fx(snd::FX_BSOD);
                    if (!dashHint) { Pop("Dash through it! (" + KeyName(set.keys[ACT_DASH]) + ")", hero.x, kGround - 280, Hex(0x1f50c8)); dashHint = true; }
                }
            }
        } else {
            sfT += dt;
            if (sfAct == 1 && sfT > 0.3f) { sfAct = 0; sfCd = (2.2f + Rand01()) * pace; }
            else if (sfAct == 2 && sfT >= 0.55f) {   // the spear, flat along the ground
                bolts.push_back({{bossX - 200, kGround - 62}, {-950, 0}});
                Fx(snd::FX_THROW);
                sfAct = 0;
                sfCd = (2.0f + Rand01()) * pace;
            } else if (sfAct == 3 && sfT >= 0.9f) {   // the blue screen
                walls.push_back(bossX - 180);
                Shake(0.3f);
                sfAct = 0;
                sfCd = (2.6f + Rand01()) * pace;
            }
        }
        // walking into it hurts
        float ddx = hero.x - bossX;
        if (std::fabs(ddx) < 150 && hero.y > kGround - 420) HurtHero(ddx > 0 ? 1 : -1);
    }

    // The Tangled List: it stands at the far side of the square and either
    // lays a list of nodes along the street and sends the pointer p hopping
    // along it, p = p->next, crushing whatever stands on the node it lands on
    // (watch which node is lit next: step off it, or jump), or cracks a chain of
    // nodes along the cobbles at you (jump).
    static constexpr float kTraverseWait = 0.8f, kTraverseHop = 0.5f;
    void UpdateTangle(float dt, float L, float vw) {
        bossX = arenaCam + L + vw * 0.76f + std::sin(bossT * 0.6f) * vw * 0.03f;
        for (Tile& tl : tiles) tl.struck = std::max(0.0f, tl.struck - dt * 3);
        if (tlAct == 0) {
            tlPose = std::max(0.0f, tlPose - dt * 2);
            tlCd -= dt;
            if (tlCd <= 0) {
                tlAct = Rand01() < 0.55f ? 1 : 2;
                tlT = 0;
                if (tlAct == 1) {   // five nodes along the street, the head on the left
                    tiles.clear();
                    float lo = arenaCam + L + 280, hi = bossX - 250;
                    for (int i = 0; i < 5; i++) tiles.push_back({lo + i * (hi - lo) / 4, GetRandomValue(1, 99), 0});
                    tlP = -1;
                    Fx(snd::FX_LINK, 0.7f);
                }
            }
        } else if (tlAct == 1) {
            tlT += dt;
            tlPose = std::min(1.0f, tlPose + dt * 3);
            int idx = tlT < kTraverseWait ? -1 : (int)((tlT - kTraverseWait) / kTraverseHop);
            if (idx != tlP) {
                tlP = idx;
                if (tlP < (int)tiles.size()) {
                    Tile& tl = tiles[tlP];
                    tl.struck = 1;
                    Fx(snd::FX_TRAVERSE, 0.8f);
                    Shake(0.15f);
                    Burst(tl.x, kGround - 20, 10, Hex(0xffc14d), 300, 2, 0, 3);
                    if (std::fabs(hero.x - tl.x) < 50 && hero.y > kGround - 40) HurtHero(hero.x > tl.x ? 1 : -1);
                } else {   // past the tail: p is NULL, the list is gone
                    tiles.clear();
                    tlAct = 0;
                    tlCd = 2.2f + Rand01() * 1.0f;
                }
            }
        } else {
            tlT += dt;
            tlPose = std::min(1.0f, tlT / 0.4f);
            if (tlT >= 0.45f) {
                rings.push_back({bossX - 150, -430, 6});
                Fx(snd::FX_WHIP);
                Shake(0.2f);
                tlAct = 0;
                tlCd = 2.0f + Rand01() * 1.0f;
            }
        }
        // walking into it hurts
        float ddx = hero.x - bossX;
        if (std::fabs(ddx) < 150 && hero.y > kGround - 320) HurtHero(ddx > 0 ? 1 : -1);
    }

    // The Leak Lord: it sits at the far side of the clearing and either spits
    // blobs of leaked memory in arcs at you (each leaves a puddle: keep moving)
    // or calls little slimes out of itself (one hit each: cut them down).
    void UpdateLeakLord(float dt, float L, float vw) {
        bossX = arenaCam + L + vw * 0.78f + std::sin(bossT * 0.5f) * vw * 0.03f;
        int minions = 0;
        for (const Goblin& g : goblins) minions += g.minion && g.alive && g.dying < 0;
        if (lordAct == 0) {
            lordMouth = std::max(0.0f, lordMouth - dt * 2.5f);
            lordCd -= dt;
            if (lordCd <= 0) { lordAct = (Rand01() < 0.6f || minions >= 3) ? 1 : 2; lordT = 0; }
        } else {
            lordT += dt;
            if (lordAct == 1) {   // three blobs: at you, a little in front, a little behind
                lordMouth = std::min(1.0f, lordT / 0.35f);
                if (lordT >= 0.4f) {
                    Vector2 m = {bossX - 60, kGround - 140};
                    for (int i = 0; i < 3; i++) {
                        float tx = hero.x + (i - 1) * 120 + (Rand01() - 0.5f) * 50, T = 0.85f + 0.12f * i;
                        float vx = (tx - m.x) / T, vy = (kGround - m.y - 0.5f * 1400 * T * T) / T;
                        blobs.push_back({m, {vx, vy}});
                    }
                    Fx(snd::FX_LOB);
                    lordAct = 0;
                    lordCd = 2.2f + Rand01() * 1.0f;
                }
            } else {   // two little slimes squeeze out of it
                lordMouth = 0.5f;
                if (lordT >= 0.5f) {
                    for (int i = 0; i < 2; i++) {
                        Goblin g;
                        g.x = g.home = bossX - 170 - i * 70.0f;
                        g.hp = g.maxHp = 1;
                        g.minion = true;
                        g.vy = 0.65f;
                        g.index = 8;
                        g.face = -1;
                        goblins.push_back(g);
                        Burst(g.x, kGround - 30, 12, Hex(0x7be36b), 300, 0, 900, 5);
                    }
                    Fx(snd::FX_SPAWN);
                    lordAct = 0;
                    lordCd = 2.6f + Rand01() * 1.0f;
                }
            }
        }
        // walking into it hurts
        float ddx = hero.x - bossX;
        if (std::fabs(ddx) < 165 && hero.y > kGround - 260) HurtHero(ddx > 0 ? 1 : -1);
    }

    // Where the Wyvern's mouth is (bolts come out of it).
    Vector2 WyvernMouth() const { return {bossX + wyFace * 150.0f, bossY - 68}; }

    // The Wild Pointer Wyvern: it hovers over the right of the arena and either
    // aims at you (a red line follows you, locks, then a bolt flies along it:
    // move!) or dives across the arena close to the ground (jump!).
    void UpdateWyvern(float dt, float L, float vw) {
        const float homeX = arenaCam + L + vw * 0.74f + std::sin(bossT * 0.6f) * vw * 0.10f;
        const float homeY = 275 + std::sin(bossT * 1.4f) * 30;   // low enough that its label stays clear of the HUD
        wyT += dt;
        if (wyAct == 0 || wyAct == 1) {   // hovering (also while aiming)
            float k = std::min(1.0f, dt * 2.5f);
            bossX += (homeX - bossX) * k;
            bossY += (homeY - bossY) * k;
            wyFace = hero.x > bossX ? 1 : -1;
            wyTilt *= 1 - std::min(1.0f, dt * 4);
        }
        if (wyAct == 0) {
            wyCd -= dt;
            if (wyCd <= 0) { wyAct = Rand01() < 0.55f ? 1 : 2; wyT = 0; if (wyAct == 2) { wyDiveX0 = bossX; wyDiveY0 = bossY; Fx(snd::FX_DIVE, 0.8f); } }
        } else if (wyAct == 1) {   // aiming: tracks you for 0.9 s, locks for a moment, fires
            if (wyT < 0.9f) aimTo = {hero.x, hero.y - 70};
            if (wyT >= 0.9f && wyT - dt < 0.9f) Fx(snd::FX_AIM, 0.7f);
            if (wyT >= 1.25f) {
                Vector2 m = WyvernMouth(), d = {aimTo.x - m.x, aimTo.y - m.y};
                float l = std::sqrt(d.x * d.x + d.y * d.y) + 1e-4f;
                bolts.push_back({m, {d.x / l * 1100, d.y / l * 1100}});
                Fx(snd::FX_BOLT);
                wyAct = 0;
                wyCd = 1.7f + Rand01() * 1.0f;
            }
        } else if (wyAct == 2) {   // the dive: up a little, then a long swoop to the left, low over the ground
            const float wind = 0.45f, len = 1.15f;
            wyFace = -1;
            if (wyT < wind) { bossY = wyDiveY0 - 50 * (wyT / wind); wyTilt = -15 * (wyT / wind); }
            else {
                float u = std::min(1.0f, (wyT - wind) / len);
                float x1 = arenaCam + L + 140, y0 = wyDiveY0 - 50, low = kGround - 75;
                bossX = wyDiveX0 + (x1 - wyDiveX0) * u;
                bossY = y0 + (low - y0) * std::sin(u * PI);   // down to just over your head, and up again
                wyTilt = 28 * std::cos(u * PI);   // nose down, then up
                float ddx = hero.x - bossX, ddy = (hero.y - 70) - bossY;
                if (std::fabs(ddx) < 80 && std::fabs(ddy) < 90) HurtHero(ddx > 0 ? 1 : -1);
                if (u >= 1) { wyAct = 3; wyT = 0; }
            }
        } else {   // flying back to its place, high up
            float k = std::min(1.0f, dt * 2.2f);
            bossX += (homeX - bossX) * k;
            bossY += (homeY - 40 - bossY) * k;
            wyFace = 1;
            wyTilt = -10;
            if (std::fabs(bossX - homeX) < 40) { wyAct = 0; wyCd = 1.6f + Rand01() * 1.0f; }
        }
    }

    // The Matrix Golem: it plods to and fro, and every few seconds raises its
    // arms, either to drop blocks where you stand (move!) or to slam the ground
    // and send a shockwave along it (jump!).
    void UpdateGolem(float dt, float L, float vw) {
        bossX = arenaCam + L + vw * 0.72f + std::sin(bossT * 0.45f) * vw * 0.10f;
        if (golemAct == 0) {
            golemPose = std::max(0.0f, golemPose - dt * 2.5f);
            golemCd -= dt;
            if (golemCd <= 0) { golemAct = Rand01() < 0.5f ? 1 : 2; golemT = 0; }
        } else {
            golemT += dt;
            golemPose = std::min(1.0f, golemT / 0.5f);
            if (golemAct == 1 && golemT >= 0.5f) {   // a rain of blocks: one where you stand, one or two elsewhere
                float lo = arenaCam + L + 240, hi = arenaCam + L + vw - 90;
                std::vector<float> xs = {std::clamp(hero.x, lo, hi)};
                int more = GetRandomValue(1, 2);
                for (int tries = 0; tries < 20 && (int)xs.size() < 1 + more; tries++) {
                    float x = lo + Rand01() * (hi - lo);
                    bool apart = true;
                    for (float o : xs) apart &= std::fabs(o - x) > 150;
                    if (apart) xs.push_back(x);
                }
                for (size_t i = 0; i < xs.size(); i++) {
                    float warn = 0.7f + 0.3f * i;
                    int r = GetRandomValue(0, 2), c = GetRandomValue(0, 2);
                    rocks.push_back({xs[i], -60, 500, warn, warn, GetRandomValue(0, 9), TextFormat("m[%d][%d]", r, c), -1});
                }
                Fx(snd::FX_SLAM, 0.4f);
                golemAct = 0;
                golemCd = 2.4f + Rand01() * 1.0f;
            }
            if (golemAct == 2 && golemT >= 0.6f) {   // the slam
                rings.push_back({bossX - 150, -380, 6});
                Fx(snd::FX_SLAM);
                shake = 0.35f;
                golemAct = 0;
                golemCd = 2.0f + Rand01() * 1.0f;
            }
        }
        // walking into it hurts
        float ddx = hero.x - bossX;
        if (std::fabs(ddx) < 125 && hero.y > kGround - 330) HurtHero(ddx > 0 ? 1 : -1);
    }

    // A sword hit on a goblin: sometimes (and always on the last hit) a question.
    void Strike(int i) {
        Goblin& g = goblins[i];
        g.hurt = 1;
        g.x += hero.face * 26;
        Burst(g.x - hero.face * 20, kGround - 60 - g.hy, 10, Hex(0xfff2c0), 480, 2, 0, 3);   // sparks
        Hit(0.05f, 0.2f);
        combo++;
        comboT = 2.2f;
        comboPop = 1;
        if (g.minion) {   // a boss's little slime: one hit
            g.hp = 0;
            g.dying = 0;
            Burst(g.x, kGround - 30, 18, Hex(kBugColor[std::clamp(land, 0, 3)]), 360, 1, 900, 5);
            Fx(snd::FX_SQUISH);
            return;
        }
        if (land == 3) Fx(snd::FX_SQUISH, 0.7f);
        bool last = g.hp <= 1;
        if (last || Rand01() < 0.4f) { AskQuestion(i, last); return; }
        g.hp--;
        Fx(snd::FX_HIT);
        Pop("hit", g.x, kGround - 120 - g.hy, Hex(0x4b3fb0));
    }

    // ---------------------------------------------------------------- quiz --
    void AskQuestion(int target, bool final) {
        if (questionPool.empty()) { goblins[target].hp = 0; goblins[target].dying = 0; return; }
        if (poolNext >= questionPool.size()) { Shuffle(questionPool); poolNext = 0; }
        q = QuestionById(questionPool[poolNext++]);
        if (!q) return;
        order.clear();
        for (int k = 0; k < (int)q->options.size(); k++) order.push_back(k);
        Shuffle(order);
        picked = -1;
        quizTarget = target;
        quizFinal = final;
        quizT = 0;
        state = QUIZ;
        Fx(snd::FX_QUIZ);
    }

    void Quiz(float dt) {
        quizT += dt;
        Shade(0.62f);
        // measure first, so the box fits what it holds
        float need = 84 + RichText({0, 0, 800, 200}, q->prompt, 19, WHITE, false) + 12;
        if (!q->code.empty()) need += SplitLines(q->code).size() * CodeLineHeight(15) + 36;
        need += order.size() * 58 + (picked != -1 ? 150 : 24) + 30;
        float bh = std::min(H - 60, need);
        Rectangle box = {W / 2 - 430, (H - bh) / 2, 860, bh};
        DrawRectangleRounded(box, 0.05f, 12, Hex(0x15122b));
        DrawRectangleRoundedLinesEx(box, 0.05f, 12, 2, quizFinal ? Hex(0xff9e4a) : Hex(0x8b7bff));
        Text(quizFinal ? "FINISHING BLOW" : "CRITICAL STRIKE", box.x + 30, box.y + 24, 15, quizFinal ? Hex(0xff9e4a) : Hex(0xb9b0ff), F_BOLD);
        Text(std::string("Answer right and the ") + kLands[land].enemy + " falls. Wrong, and it strikes back.", box.x + 30, box.y + 46, 13.5f, Hex(0x8f88b8), F_MEDIUM);
        const float limit = 25;
        if (set.questionTimer && picked < 0) {
            float left = std::max(0.0f, limit - quizT);
            Rectangle bar = {box.x + box.width - 230, box.y + 30, 200, 8};
            DrawRectangleRounded(bar, 1, 8, Hex(0x2a2550));
            DrawRectangleRounded({bar.x, bar.y, bar.width * left / limit, bar.height}, 1, 8, left < 6 ? Hex(0xff5c7a) : Hex(0x8b7bff));
            TextRight(std::to_string((int)std::ceil(left)) + " s", bar.x - 10, box.y + 24, 15, Hex(0xd8d3f5), F_BOLD);
            if (left <= 0) Answer(-2);
        }
        float y = box.y + 84;
        y += RichText({box.x + 30, y, box.width - 60, 200}, q->prompt, 19, Hex(0xf0edff)) + 12;
        if (!q->code.empty()) {
            std::vector<std::string> lines = SplitLines(q->code);
            float lh = CodeLineHeight(15);
            Rectangle code = {box.x + 30, y, box.width - 60, lines.size() * lh + 20};
            DrawRectangleRounded(code, 0.06f, 8, Hex(0x0b0a14));
            for (size_t i = 0; i < lines.size(); i++) CodeLine(code.x + 16, code.y + 10 + i * lh, lines[i], 15);
            y += code.height + 16;
        }
        // the options: 1-4 or a click
        for (size_t i = 0; i < order.size(); i++) {
            Rectangle r = {box.x + 30, y, box.width - 60, 50};
            bool isRight = order[i] == 0;
            bool hover = picked < 0 && MouseIn(r);
            Color fill = Hex(0x221d42), line = Hex(0x3b3470);
            if (picked >= 0 && isRight) { fill = Hex(0x1d4a35); line = Hex(0x48e5a0); }
            else if (picked == (int)i) { fill = Hex(0x4a1d2c); line = Hex(0xff5c7a); }
            else if (hover) { fill = Hex(0x2d2756); line = Hex(0x8b7bff); }
            DrawRectangleRounded(r, 0.3f, 8, fill);
            DrawRectangleRoundedLinesEx(r, 0.3f, 8, 2, line);
            DrawRectangleRounded({r.x + 12, r.y + 12, 26, 26}, 0.4f, 6, Hex(0x3b3470));
            TextCenter(std::to_string(i + 1), {r.x + 12, r.y + 12, 26, 26}, 15, WHITE, F_BOLD);
            RichText({r.x + 52, r.y + 13, r.width - 70, 30}, q->options[order[i]], 16.5f, Hex(0xe6e1ff));
            if (hover) WantHand();
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) Answer((int)i);
            if (picked < 0 && IsKeyPressed(KEY_ONE + (int)i)) Answer((int)i);
            y += 58;
        }
        if (picked != -1) {
            Text(quizRight ? "Right!" : (picked == -2 ? "Out of time!" : "Wrong!"), box.x + 30, y + 4, 22, quizRight ? Hex(0x48e5a0) : Hex(0xff5c7a), F_HEAVY);
            if (!q->explain.empty()) RichText({box.x + 30, y + 36, box.width - 60, 120}, q->explain, 15, Hex(0xb9b3d9));
            TextCenter("Enter / click to continue", {box.x, box.y + box.height - 34, box.width, 20}, 13, Hex(0x8f88b8), F_MEDIUM);
            bool go = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && GetTime() - answeredAt > 0.25);
            if (go) ResolveQuiz();
        }
    }

    void Answer(int i) {
        if (picked != -1) return;
        picked = i;
        quizRight = i >= 0 && order[i] == 0;
        answeredAt = (float)GetTime();
        Fx(quizRight ? snd::FX_RIGHT : snd::FX_WRONG);
    }

    void ResolveQuiz() {
        state = PLAY;
        Goblin& g = goblins[quizTarget];
        if (quizRight) {
            g.hp = quizFinal ? 0 : std::max(0, g.hp - 2);   // a critical hit
            Pop(quizFinal ? "Finished!" : "CRITICAL!", g.x, kGround - 140, Hex(0xe07800));
            if (g.hp <= 0) {
                bool wasHead = land == 4 && Ahead(g) < 0 && Behind(g) >= 0;
                g.dying = 0;
                Fx(snd::FX_KILL);
                if (wasHead) {   // the list goes on without it
                    Pop("head = head->next", g.x, kGround - 190, Hex(0x1d6c7c));
                    Fx(snd::FX_LINK);
                }
                Burst(g.x, kGround - 60 - g.hy, 34, Hex(kBugColor[std::clamp(land, 0, 3)]), 560, 1, 900, 6);
            } else Fx(snd::FX_CRIT);
            Burst(g.x, kGround - 70 - g.hy, 16, Hex(0xffd27a), 620, 2, 0, 4);
            Hit(quizFinal ? 0.14f : 0.1f, 0.45f);
            combo++;
            comboT = 2.2f;
            comboPop = 1;
        } else {
            g.hp = std::min(g.maxHp, g.hp + 1);   // it shrugs the hit off and strikes back
            g.touchCd = 1.0f;
            HurtHero(hero.x > g.x ? 1 : -1);
        }
    }

    // ------------------------------------------------------- boss terminal --
    void Terminal() {
        Draw();
        Shade(0.7f);
        Rectangle top = {W / 2 - 600, 14, 1200, 46};
        DrawRectangleRounded(top, 0.4f, 10, Hex(0x15122b));
        Text("TERMINAL", top.x + 20, top.y + 13, 17, Hex(0x48e5ff), F_MONOBOLD);
        Text(std::string("Every test your code passes hits ") + kLands[land].bossShort + ". A compile error lets it strike you.", top.x + 140, top.y + 15, 14,
             Hex(0xb9b3d9), F_MEDIUM);
        // the boss's health
        Rectangle hb = {top.x + top.width - 330, top.y + 17, 200, 12};
        DrawRectangleRounded(hb, 1, 8, Hex(0x2a2550));
        DrawRectangleRounded({hb.x, hb.y, hb.width * bossHp, hb.height}, 1, 8, Hex(kLands[land].bossColor));
        Rectangle back = {top.x + top.width - 116, top.y + 7, 104, 32};
        bool bh = MouseIn(back);
        DrawRectangleRounded(back, 0.4f, 8, bh ? Hex(0x3b3470) : Hex(0x2a2550));
        TextCenter("Back (Esc)", back, 14, WHITE, F_BOLD);
        if (bh) WantHand();
        if (!termMsg.empty()) TextCenter(termMsg, {0, H - 26, W, 20}, 14, Hex(0xffc14d), F_BOLD);

        ws.Frame({W / 2 - 600, 70, 1200, H - 100});
        // a new result: hit the boss for every test that passes (only progress counts)
        if (ws.Running()) wsRunning = true;
        else if (wsRunning && ws.HasReport()) {
            wsRunning = false;
            const RunReport& r = ws.Report();
            if (r.status == RunReport::CompileError || r.status == RunReport::RuntimeError || r.status == RunReport::TimeLimit) {
                termMsg = std::string(r.status == RunReport::CompileError ? "It doesn't compile - " : "Your program crashed - ") + kLands[land].bossShort + " strikes!";
                Fx(snd::FX_COMPILE_ERROR);
                hero.hurt = 0;
                HurtHero(-1);
                if (state == DEAD) return;
            } else if (r.total > 0 && r.passed > bestPassed) {
                int gained = r.passed - bestPassed;
                bestPassed = r.passed;
                bossHp = 1 - (float)bestPassed / r.total;
                bossHurt = 1;
                Fx(snd::FX_BOSS_HIT);
                termMsg = std::to_string(gained) + (gained == 1 ? " new test passes" : " new tests pass") + " - a direct hit!";
                if (bestPassed >= r.total) { bossDying = 0; golemPose = 0; state = PLAY; snd::StopMusic(); Fx(snd::FX_BOSS_DOWN); shake = 1.0f; return; }
            } else termMsg = "No new test passes yet. Look at the failing case and try again.";
        }
        if (bh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { state = PLAY; termCd = 1.5f; }
        else if (IsKeyPressed(KEY_ESCAPE) && !AnyTextFocused()) { state = PLAY; termCd = 1.5f; }
    }

    void Win() {
        Fx(snd::FX_VICTORY);
        chaptersDone = std::max(chaptersDone, land + 1);
        Save();
        state = VICTORY;
        deadT = 0;
    }

    // ------------------------------------------------------------ chapters --
    // Every land reached so far, as a card with its painting: play any of them again.
    void Chapters() {
        TitleBackdrop();
        Shade(0.72f);
        TextCenter("Chapters", {0, 50, W, 50}, 38, WHITE, F_HEAVY);
        TextCenter("Play any land you have reached again", {0, 98, W, 22}, 15, Hex(0xb9b3d9), F_MEDIUM);
        const float cw = 360, ch = 200, gap = 24, x0 = W / 2 - (3 * cw + 2 * gap) / 2, y0 = 150;
        int open = std::min(chaptersDone, kLandCount - 1);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { chapSel = std::min(kLandCount - 1, chapSel + 1); Fx(snd::FX_MENU_MOVE); }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { chapSel = std::max(0, chapSel - 1); Fx(snd::FX_MENU_MOVE); }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { chapSel = std::min(kLandCount - 1, chapSel + 3); Fx(snd::FX_MENU_MOVE); }
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { chapSel = std::max(0, chapSel - 3); Fx(snd::FX_MENU_MOVE); }
        int chosen = -1;
        for (int i = 0; i < kLandCount; i++) {
            Rectangle r = {x0 + (i % 3) * (cw + gap), y0 + (i / 3) * (ch + gap), cw, ch};
            bool unlocked = i <= open, hover = MouseIn(r);
            if (hover) chapSel = i;
            bool on = chapSel == i;
            BeginClip(r);
            DrawRectangleRec(r, Hex(0x15122b));
            if (unlocked) fp::DrawBackdropIn(r, i, t + i * 7);
            else DrawRectangleRec(r, Fade(Hex(0x0b0a14), 0.6f));
            EndClip();
            DrawRectangleRoundedLinesEx(r, 0.06f, 8, on ? 3.0f : 1.5f, on ? Hex(0xffc14d) : Hex(0x4b4475));
            Text(TextFormat("CHAPTER %d", i + 1), r.x + 16, r.y + 14, 13, unlocked ? Hex(0xffd29a) : Hex(0x6b648f), F_BOLD);
            Text(unlocked ? kLands[i].name : "Locked", r.x + 16, r.y + ch - 62, 24, unlocked ? WHITE : Hex(0x8f88b8), F_HEAVY);
            Text(unlocked ? kLands[i].topic : "Beat the land before it", r.x + 16, r.y + ch - 30, 13.5f, unlocked ? Hex(0xd8d3f5) : Hex(0x6b648f), F_MEDIUM);
            if (!unlocked) {   // a padlock
                Vector2 c = {r.x + cw - 40, r.y + 40};
                DrawRing({c.x, c.y - 8}, 8, 12, 180, 360, 16, Hex(0x8f88b8));
                DrawRectangleRounded({c.x - 14, c.y - 8, 28, 22}, 0.3f, 6, Hex(0x8f88b8));
            }
            if (unlocked && hover) WantHand();
            if (unlocked && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) chosen = i;
        }
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && chapSel <= open) chosen = chapSel;
        if (chosen >= 0) { Fx(snd::FX_MENU_SELECT); StartChapter(chosen); return; }
        if (Item(W / 2, 640, "Back", false) || IsKeyPressed(KEY_ESCAPE)) { state = MENU; menuSel = 0; }
    }

    // ------------------------------------------------------------- credits --
    void Credits(float dt) {
        creditsT += dt;
        Rectangle scr = ScreenRect();
        fp::DrawBackdropIn(scr, 0, t);
        DrawRectangleRec(scr, Fade(Hex(0x0b0a14), 0.5f));
        // the two of them, home at last
        fp::SetLandLight(0);
        fp::DrawShadow({scr.x + 150, H - 40}, 40, 0);
        fp::DrawShadow({scr.x + 250, H - 40}, 34, 0);
        fp::DrawHero({scr.x + 150, H - 40}, 1.1f, 1, 0, -1, t, false);
        fp::DrawAda({scr.x + 250, H - 40}, 1.0f, t);
        struct Line { const char* text; float size; unsigned col; FontKind font; };
        const Line lines[] = {
            {"THE FORGOTTEN POINTER", 44, 0xffffff, F_HEAVY}, {"A SegFault story", 18, 0xd3ccff, F_MEDIUM}, {"", 30, 0, F_UI},
            {"THE LANDS", 14, 0xffd29a, F_BOLD}, {"Stack Plains  -  the Infinite Loop Serpent", 17, 0xe6e1ff, F_MEDIUM},
            {"Array Canyon  -  the Matrix Golem", 17, 0xe6e1ff, F_MEDIUM}, {"Pointer Peaks  -  the Wild Pointer Wyvern", 17, 0xe6e1ff, F_MEDIUM},
            {"Memory Forest  -  the Leak Lord", 17, 0xe6e1ff, F_MEDIUM}, {"Linked List City  -  the Tangled List", 17, 0xe6e1ff, F_MEDIUM},
            {"The Heap  -  the SEGFAULT", 17, 0xe6e1ff, F_MEDIUM}, {"", 30, 0, F_UI},
            {"MADE BY", 14, 0xffd29a, F_BOLD}, {"The SegFault team", 20, 0xffffff, F_BOLD}, {"", 30, 0, F_UI},
            {"PAINTINGS", 14, 0xffd29a, F_BOLD}, {"Ombremonde  -  Landscape Illustrations (CC0)", 17, 0xe6e1ff, F_MEDIUM}, {"", 20, 0, F_UI},
            {"MUSIC", 14, 0xffd29a, F_BOLD}, {"Aureolus_Omicron  -  15 Melodic RPG Chiptunes (CC0)", 17, 0xe6e1ff, F_MEDIUM}, {"", 20, 0, F_UI},
            {"BUILT WITH", 14, 0xffd29a, F_BOLD}, {"raylib  -  Inter  -  JetBrains Mono", 17, 0xe6e1ff, F_MEDIUM}, {"", 40, 0, F_UI},
            {"Thank you for playing.", 22, 0xffffff, F_BOLD}, {"Every pointer remembered. Every block freed.", 17, 0xd3ccff, F_MEDIUM},
            {"", 60, 0, F_UI}, {"THE END", 52, 0xffc14d, F_HEAVY},
        };
        const int n = (int)(sizeof(lines) / sizeof(lines[0]));
        float total = 0;
        for (int i = 0; i < n; i++) total += lines[i].size + 22;
        // it scrolls up until THE END is in the middle, then stays
        float y = std::max(H / 2 - total + 52 + 22, H + 20 - creditsT * 42);
        for (int i = 0; i < n; i++) {
            if (lines[i].text[0] && y > -60 && y < H + 20)
                TextCenter(lines[i].text, {0, y, W, lines[i].size + 6}, lines[i].size, Hex(lines[i].col), lines[i].font);
            y += lines[i].size + 22;
        }
        TextRight("Enter / click to return to the menu", scr.x + scr.width - 24, 20, 13, Hex(0xb9b3d9), F_MEDIUM);
        if (creditsT > 1.0f && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            state = MENU;
            menuSel = 0;
            snd::PlayMusic("title");
        }
    }

    void Victory(float dt) {
        deadT += dt;
        Draw();
        Shade(Clamp01(deadT / 0.8f) * 0.8f);
        TextCenter(land == kLandCount - 1 ? std::string("THE KERNEL IS SAVED") : std::string(TextFormat("CHAPTER %d CLEARED", land + 1)),
                   {0, 300, W, 60}, 52, Fade(Hex(0x48e5a0), Clamp01(deadT)), F_HEAVY);
        TextCenter(std::string("The ") + kLands[land].boss + " is defeated.", {0, 370, W, 26}, 19, Fade(Hex(0xd8d3f5), Clamp01(deadT)), F_MEDIUM);
        if (deadT > 1.2f && (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || deadT > 4)) {
            if (land == 0) StartStory(kAfterChapter1, 3, CHAPTER, 1);        // on to the Array Canyon
            else if (land == 1) StartStory(kAfterChapter2, 3, CHAPTER, 2);   // on to the Pointer Peaks
            else if (land == 2) StartStory(kAfterChapter3, 3, CHAPTER, 3);   // on to the Memory Forest
            else if (land == 3) StartStory(kAfterChapter4, 3, CHAPTER, 4);   // on to Linked List City
            else if (land == 4) StartStory(kAfterChapter5, 3, CHAPTER, 5);   // down into the Heap
            else StartStory(kAfterChapter6, 4, CREDITS);                     // the ending
        }
    }

    // --------------------------------------------------- pause and death --
    void Pause() {
        Shade(0.6f);
        TextCenter("Paused", {0, 200, W, 60}, 48, WHITE, F_HEAVY);
        const char* items[3] = {"Resume", "Settings", "Main menu"};
        if (IsKeyPressed(KEY_DOWN)) { menuSel = (menuSel + 1) % 3; Fx(snd::FX_MENU_MOVE); }
        if (IsKeyPressed(KEY_UP)) { menuSel = (menuSel + 2) % 3; Fx(snd::FX_MENU_MOVE); }
        int chosen = -1;
        for (int i = 0; i < 3; i++) {
            float y = 300 + i * 64;
            if (MouseIn({W / 2 - 180, y, 360, 54})) menuSel = i;
            if (Item(W / 2, y, items[i], i == menuSel)) chosen = i;
        }
        if (IsKeyPressed(KEY_ENTER)) chosen = menuSel;
        if (IsKeyPressed(KEY_ESCAPE)) chosen = 0;
        if (chosen == 0) state = PLAY;
        if (chosen == 1) { settingsBack = PAUSE; state = SETTINGS; rebinding = -1; }
        if (chosen == 2) { state = MENU; menuSel = 0; snd::PlayMusic("title"); }
    }

    void Dead(float dt) {
        deadT += dt;
        Shade(Clamp01(deadT / 0.6f) * 0.85f);
        float a = Clamp01(deadT / 0.6f);
        TextCenter("KERNEL PANIC", {0, 250, W, 70}, 64, Fade(Hex(0xff3d5a), a), F_HEAVY);
        TextCenter("The Debugger has fallen. The bugs win this round.", {0, 330, W, 26}, 19, Fade(Hex(0xd8d3f5), a), F_MEDIUM);
        if (deadT < 0.8f) return;
        if (Item(W / 2, 400, "Try again", menuSel == 0)) StartChapter(land);
        if (Item(W / 2, 464, "Main menu", menuSel == 1)) { state = MENU; menuSel = 0; snd::PlayMusic("title"); }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_UP)) menuSel = 1 - std::clamp(menuSel, 0, 1);
        if (IsKeyPressed(KEY_ENTER)) { Fx(snd::FX_MENU_SELECT); if (menuSel == 0) StartChapter(land); else { state = MENU; menuSel = 0; snd::PlayMusic("title"); } }
    }

    // ------------------------------------------------------------- drawing --
    void Draw() {
        Rectangle scr = ScreenRect();
        // shake: strong at once, then quickly gone (it's the square of what is left)
        float amount = std::min(1.0f, shake);
        float sx = set.shake && shake > 0 ? (std::sin(t * 83) + std::sin(t * 51.7f)) * 0.5f * amount * amount * 26 : 0;
        float c = cam - sx;
        fp::SetLandLight(land);
        if (land == 0) fp::DrawStackPlains(c, t);
        else if (land == 1) fp::DrawArrayCanyon(c, t);
        else if (land == 2) fp::DrawPointerPeaks(c, t);
        else if (land == 3) fp::DrawMemoryForest(c, t);
        else if (land == 4) fp::DrawListCity(c, t);
        else fp::DrawTheHeap(c, t);
        auto X = [&](float wx) { return wx - c; };
        // signposts along the way: addresses on the plains, indices in the canyon
        int post = 0;
        for (float wx = 600; wx < kLevelLen - 1200; wx += 1400, post++) {
            float x = X(wx);
            if (x < scr.x - 100 || x > scr.x + scr.width + 100) continue;
            DrawRectangle((int)x - 4, (int)kGround - 90, 8, 90, Hex(0x7a4a2a));
            DrawRectangleRounded({x - 60, kGround - 120, 120, 40}, 0.2f, 6, Hex(0xe8d3a8));
            const char* label = land == 0 ? TextFormat("0x%04X", (int)wx) : land == 1 ? TextFormat("arr[%d]", post)
                              : land == 2 ? TextFormat("p + %d", post)
                              : land == 3 ? TextFormat("heap #%d", post)
                              : land == 4 ? TextFormat("n%d->next", post) : TextFormat("0x7FF%03X", post * 0x120);
            TextCenter(label, {x - 60, kGround - 112, 120, 24}, 16, Hex(0x5a3a1a), F_MONOBOLD);
        }
        // the arena: the terminal and the boss
        if (arena) {
            fp::DrawTerminalPillar({X(termX), kGround}, t, termCd <= 0 && bossDying < 0, std::fabs(hero.x - termX) < 90);
            if (bossDying < 0 && std::fabs(hero.x - termX) < 90 && termCd <= 0)
                TextCenter("Press " + KeyName(set.keys[ACT_USE]) + " to write code", {X(termX) - 150, kGround - 250, 300, 22}, 16, Hex(0x48e5ff), F_BOLD);
        }
        if (bossAwake && land == 0) {
            Vector2 bc = BossCenter();
            float alpha = bossDying >= 0 ? 1 - Clamp01(bossDying / 2.2f) : 1;
            float radius = 130 + (bossDying >= 0 ? bossDying * 40 : 0);
            fp::DrawShadow({X(bc.x), kGround}, 150 * alpha, 0.55f);
            fp::DrawSerpent({X(bc.x), bc.y}, radius, t, bossHurt, alpha);
        }
        if (bossAwake && land == 1) {   // it falls apart, then fades
            float crumble = bossDying >= 0 ? Clamp01(bossDying / 1.4f) : 0;
            float alpha = bossDying >= 0 ? 1 - Clamp01((bossDying - 1.4f) / 0.9f) : 1;
            fp::DrawShadow({X(bossX), kGround}, 150 * alpha, 0);
            fp::DrawGolem({X(bossX), kGround}, 1.0f, t, golemPose, bossHurt, alpha, crumble);
        }
        if (bossAwake && land == 2) {   // when beaten it falls out of the sky, then fades
            float y = bossY, tilt = wyTilt, alpha = 1;
            if (bossDying >= 0) {
                y = std::min(kGround - 70, bossY + bossDying * bossDying * 260);
                tilt = std::min(70.0f, bossDying * 90);
                alpha = 1 - Clamp01((bossDying - 1.4f) / 0.9f);
            }
            float wing = std::fmod(t * (wyAct == 2 ? 2.6f : 1.5f), 1.0f);
            fp::DrawShadow({X(bossX), kGround}, 130 * alpha, Clamp01((kGround - y) / 600));
            fp::DrawWyvern({X(bossX), y}, 1.0f, wyFace, tilt, bossDying >= 0 ? 0.25f : wing, bossHurt, alpha, t);
        }
        if (bossAwake && land == 3) {   // when beaten it melts into the moss
            float melt = bossDying >= 0 ? Clamp01(bossDying / 1.6f) : 0;
            float alpha = bossDying >= 0 ? 1 - Clamp01((bossDying - 1.5f) / 0.8f) : 1;
            fp::DrawShadow({X(bossX), kGround}, 190 * alpha, 0);
            fp::DrawLeakLord({X(bossX), kGround}, 1.0f, t, lordMouth, bossHurt, alpha, melt);
        }
        if (bossAwake && land == 4) {   // when beaten it comes undone and sinks
            float fall = bossDying >= 0 ? Clamp01(bossDying / 1.6f) : 0;
            float alpha = bossDying >= 0 ? 1 - Clamp01((bossDying - 1.5f) / 0.8f) : 1;
            fp::DrawShadow({X(bossX), kGround}, 220 * alpha, 0);
            fp::DrawTangledList({X(bossX), kGround}, 1.0f, t, tlPose, bossHurt, alpha, fall);
        }
        if (bossAwake && land == 5) {   // when beaten it glitches, shakes and fades out
            float alpha = bossDying >= 0 ? 1 - Clamp01((bossDying - 1.2f) / 1.0f) : 1;
            float jitter = bossDying >= 0 ? std::sin(t * 70) * 10 * Clamp01(bossDying) : 0;
            fp::DrawShadow({X(bossX), kGround}, 200 * alpha, 0);
            fp::DrawSegfault({X(bossX) + jitter, kGround - 116}, 0.75f, t, bossHurt, alpha);
        }
        for (const Beam& b : beams) {
            if (b.warn > 0) fp::DrawBeamMark(X(b.x), 1 - b.warn / b.warn0, t);
            else fp::DrawBeam(X(b.x), b.life, t);
        }
        // the traverse: the list on the street, the next node to be hit lit up, p over it
        for (size_t i = 0; i < tiles.size(); i++) {
            if (i + 1 < tiles.size()) fp::DrawLink({X(tiles[i].x) + 28, kGround - 15}, {X(tiles[i + 1].x) - 46, kGround - 15}, 1, t);
            float warn = 0;
            if ((int)i == tlP + 1) warn = tlP < 0 ? Clamp01(tlT / kTraverseWait) : Clamp01((tlT - kTraverseWait - tlP * kTraverseHop) / kTraverseHop);
            fp::DrawListTile(X(tiles[i].x), tiles[i].value, warn, tiles[i].struck, t);
        }
        if (!tiles.empty()) {
            int at = std::clamp(tlP, 0, (int)tiles.size() - 1);
            Text("NULL", X(tiles.back().x) + 60, kGround - 26, 15, Hex(0xff8a9c), F_MONOBOLD);
            fp::DrawCursorP({X(tiles[at].x), kGround - 40}, t);
        }
        for (const Puddle& p : puddles) fp::DrawPuddle(X(p.x), p.life, t);
        for (const Bolt& b : blobs) fp::DrawBlob({X(b.p.x), b.p.y}, t);
        for (const Bolt& b : bolts) {
            if (land == 5) { fp::DrawSpear({X(b.p.x), b.p.y}, b.v.x > 0 ? 1.0f : -1.0f, t); continue; }
            float l = std::sqrt(b.v.x * b.v.x + b.v.y * b.v.y) + 1e-4f;
            fp::DrawBolt({X(b.p.x), b.p.y}, {b.v.x / l, b.v.y / l}, t);
        }
        for (const Ring& r : rings) {
            if (land == 4) { fp::DrawChainWhip({X(r.x), kGround}, t); continue; }
            if (land == 0) fp::DrawLoopRing({X(r.x), kGround - 26}, 26, t);
            else fp::DrawShockwave({X(r.x), kGround}, t);
        }
        for (const Rock& r : rocks) {
            if (r.landed < 0) fp::DrawDropMarker(X(r.x), t, r.warn > 0 ? 1 - r.warn / r.warn0 : 1, r.label.c_str());
            if (r.warn <= 0) fp::DrawStoneBlock({X(r.x), r.y}, 74, r.value, r.landed >= 0 ? 1 - Clamp01(r.landed / 0.5f) : 1);
        }
        // goblins
        for (const Goblin& g : goblins) {
            if (!g.alive) continue;
            float a = g.dying >= 0 ? 1 - Clamp01(g.dying / 0.6f) : 1;
            fp::DrawShadow({X(g.x), kGround}, (land == 3 ? 46 * g.vy : land == 4 ? 40 : land == 5 ? 30 : 34) * a, g.hy / 200);
            if (land == 0) fp::DrawGoblin({X(g.x), kGround + (g.dying >= 0 ? g.dying * 30 : 0)}, 1.0f, g.face, g.walk, g.hurt, a);
            else if (land == 1) fp::DrawImp({X(g.x), kGround - g.hy + (g.dying >= 0 ? g.dying * 30 : 0)}, 1.0f, g.face, g.hy / 100, g.hurt, a, g.index, t);
            else if (land == 2) fp::DrawGhost({X(g.x), kGround - g.hy + (g.dying >= 0 ? g.dying * 30 : 0)}, 1.0f, g.face, g.hurt, a, g.vy, t + g.home);
            else if (land == 5) fp::DrawBat({X(g.x), kGround - g.hy - 20 + (g.dying >= 0 ? g.dying * 60 : 0)}, 1.0f, g.face, g.walk, g.hurt, a, t);
            else if (land == 4) {   // a node faces back along its line, so its next points at the one behind
                Vector2 f = {X(g.x), kGround + (g.dying >= 0 ? g.dying * 30 : 0)};
                int behind = g.dying < 0 ? Behind(g) : -1;
                if (behind >= 0) fp::DrawLink(fp::NodeNextDot(f, 1.0f, -g.face), fp::NodeInlet({X(goblins[behind].x), kGround}, 1.0f, -goblins[behind].face), a, t);
                fp::DrawNode(f, 1.0f, -g.face, g.walk, g.hurt, a, g.index, g.dying < 0 && Ahead(g) < 0, behind < 0, t);
            }
            else fp::DrawSlime({X(g.x), kGround + (g.dying >= 0 ? g.dying * 20 : 0)}, 1.0f, g.face, 0, g.vy, g.hurt, a, g.index, t + g.home);
            if (g.dying < 0 && g.hp < g.maxHp) {   // a small health bar once hit
                float top = land == 0 ? 140 : land == 1 ? 168 : land == 2 ? 150 : land == 3 ? 50 * g.vy + 50 : land == 4 ? 104 : 74;
                Rectangle hb = {X(g.x) - 24, kGround - top - g.hy, 48, 6};
                DrawRectangleRounded(hb, 1, 6, Hex(0x2a2550));
                DrawRectangleRounded({hb.x, hb.y, hb.width * g.hp / g.maxHp, hb.height}, 1, 6, Hex(0x6fd15a));
            }
        }
        // the hero (blinks while hurt)
        bool blink = hero.hurt > 0 && std::fmod(hero.hurt, 0.16f) < 0.08f;
        fp::DrawShadow({X(hero.x), kGround}, 40, (kGround - hero.y) / 220);
        {   // squash and stretch, around its feet
            float sq = 1 + hero.squash;
            rlPushMatrix();
            rlTranslatef(X(hero.x), hero.y, 0);
            rlScalef(1 / sq, sq, 1);
            rlTranslatef(-X(hero.x), -hero.y, 0);
            fp::DrawHero({X(hero.x), hero.y}, 1.0f, hero.face, hero.walk, hero.attack, t, blink && hero.dash <= 0);
            rlPopMatrix();
        }
        // particles over everything in the world
        for (const Particle& p : parts) {
            float k = p.life / p.max;
            Color col = Fade(p.c, (p.c.a / 255.0f) * std::min(1.0f, k * 1.6f));
            if (p.kind == 0) DrawCircleV({X(p.x), p.y}, p.size * (0.5f + 0.5f * (1 - k) + 0.3f), col);
            else if (p.kind == 1) DrawRectanglePro({X(p.x), p.y, p.size, p.size}, {p.size / 2, p.size / 2}, p.life * 720, col);
            else {
                float l = std::sqrt(p.vx * p.vx + p.vy * p.vy) + 1e-3f;
                float len = std::max(8.0f, l * 0.05f);
                DrawLineEx({X(p.x), p.y}, {X(p.x) - p.vx / l * len, p.y - p.vy / l * len}, p.size * k + 1, col);
            }
        }
        // the Wyvern's aim goes over the hero, so its target can be seen
        if (bossAwake && land == 2 && bossDying < 0 && wyAct == 1)
            fp::DrawAimLine({X(WyvernMouth().x), WyvernMouth().y}, {X(aimTo.x), aimTo.y}, wyT < 0.9f ? 0.0f : 1.0f, t);
        for (const Popup& p : popups)
        {
            float a = 1 - p.age / 1.4f;
            TextCenter(p.text, {X(p.x) - 198, p.y - p.age * 50 + 2, 400, 30}, 22, Fade(Hex(0x0b0918), 0.6f * a), F_HEAVY);
            TextCenter(p.text, {X(p.x) - 200, p.y - p.age * 50, 400, 30}, 22, Fade(p.col, a), F_HEAVY);
        }
        // the blue screens, in front of everything
        for (float w : walls) fp::DrawBsodWave(X(w), t);
        if (bossAwake && land == 5 && sfAct == 3) {   // its warning: the right of the screen flickers blue
            Rectangle s2 = ScreenRect();
            float k = Clamp01(sfT / 0.9f) * (0.5f + 0.5f * std::sin(t * 30));
            DrawRectangleGradientH((int)(s2.x + s2.width) - 220, 0, 221, (int)H, Fade(Hex(0x1f50c8), 0), Fade(Hex(0x1f50c8), 0.6f * k));
        }
        // a red edge when the hero is hurt
        if (hurtFlash > 0) {
            Rectangle scr2 = ScreenRect();
            Color red = Fade(Hex(0xff2a4a), 0.55f * hurtFlash), none = Fade(Hex(0xff2a4a), 0);
            DrawRectangleGradientV((int)scr2.x, 0, (int)scr2.width + 1, 120, red, none);
            DrawRectangleGradientV((int)scr2.x, (int)H - 120, (int)scr2.width + 1, 120, none, red);
            DrawRectangleGradientH((int)scr2.x, 0, 120, (int)H, red, none);
            DrawRectangleGradientH((int)(scr2.x + scr2.width) - 120, 0, 121, (int)H, none, red);
        }
        Hud();
    }

    void Hud() {
        Rectangle scr = ScreenRect();
        DrawRectangleGradientV((int)scr.x, 0, (int)scr.width + 1, 96, Fade(Hex(0x120f26), 0.45f), Fade(Hex(0x120f26), 0));
        float x = scr.x + 24;
        for (int i = 0; i < set.Hearts(); i++) {
            bool full = i < hero.hearts;
            Vector2 c = {x + i * 34.0f + 12, 34};
            Color col = full ? Hex(0xff4d6d) : Hex(0x3b3470);
            DrawCircleV({c.x - 6, c.y - 3}, 8, col);
            DrawCircleV({c.x + 6, c.y - 3}, 8, col);
            DrawTriangle({c.x - 13.5f, c.y}, {c.x, c.y + 15}, {c.x + 13.5f, c.y}, col);
        }
        int left = 0;
        for (const Goblin& g : goblins) left += g.alive && g.dying < 0 && !g.minion;
        if (combo >= 2) {   // the combo, under the hearts
            float k = 1 + 0.35f * comboPop;
            std::string cs = "x" + std::to_string(combo) + " COMBO";
            float fs2 = std::round(22 * k);
            Text(cs, scr.x + 25, 62, fs2, Fade(Hex(0x0b0918), 0.6f), F_HEAVY);
            Text(cs, scr.x + 24, 60, fs2, combo >= 5 ? Hex(0xffc14d) : Hex(0xffffff), F_HEAVY);
            DrawRectangleRounded({scr.x + 24, 60 + fs2 + 6, 120 * Clamp01(comboT / 2.2f), 4}, 1, 6, Hex(0xffc14d));
        }
        const Land& ld = kLands[land];
        std::string info = arena ? std::string("Defeat the ") + ld.boss
                                 : (left > 0 ? std::to_string(left) + " " + (left == 1 ? ld.enemy : ld.enemies) + " ahead" : "Go right, to the guardian");
        const Color sh = Fade(Hex(0x0b0918), 0.6f);
        TextRight(ld.name, scr.x + scr.width - 23, 19, 18, sh, F_HEAVY);
        TextRight(info, scr.x + scr.width - 23, 43, 13.5f, sh, F_BOLD);
        TextRight(ld.name, scr.x + scr.width - 24, 18, 18, WHITE, F_HEAVY);
        TextRight(info, scr.x + scr.width - 24, 42, 13.5f, WHITE, F_BOLD);
        if (bossAwake && bossDying < 0) {
            Rectangle hb = {W / 2 - 220, 22, 440, 16};
            DrawRectangleRounded(hb, 1, 8, Hex(0x2a2550));
            DrawRectangleRounded({hb.x, hb.y, hb.width * bossHp, hb.height}, 1, 8, Hex(ld.bossColor));
            TextCenter(ld.boss, {hb.x + 1, hb.y + 21, hb.width, 18}, 14, Fade(Hex(0x0b0918), 0.6f), F_BOLD);
            TextCenter(ld.boss, {hb.x, hb.y + 20, hb.width, 18}, 14, WHITE, F_BOLD);
        }
        if (!arena && hero.x < 800 && state == PLAY)
            TextCenter(KeyName(set.keys[ACT_LEFT]) + "/" + KeyName(set.keys[ACT_RIGHT]) + " to move, " + KeyName(set.keys[ACT_JUMP]) +
                           " to jump, " + KeyName(set.keys[ACT_ATTACK]) + " to attack. Esc pauses.",
                       {0, H - 40, W, 20}, 14, Hex(0xd8d3f5), F_MEDIUM);
    }
};

} // namespace

Scene* CreateGameScene() { return new GameScene(); }
