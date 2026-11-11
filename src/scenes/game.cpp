// ============================================================================
// game.cpp  -  The Forgotten Pointer: a side-scrolling adventure on the
// SegFault OS desktop. Walk through each land, and when you strike a bug a
// question may appear: answer it right for a critical hit, wrong and the bug
// strikes back. Each land ends with a boss you beat by writing code: every
// test your code passes is a hit.
//
// Phase 1: title menu, settings, the story, and Chapter 1 (Stack Plains:
// Off-by-One Goblins and the Infinite Loop Serpent).
// ============================================================================
#include "app/app.h"
#include "app/theme.h"
#include "app/ui.h"
#include "content/content.h"
#include "scenes/game_art.h"
#include "scenes/workspace.h"
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

// ---------------------------------------------------------------- sounds ---
// Short tones made in code (no sound files), played at the chosen volume.
struct Sfx {
    Sound hit{}, hurt{}, right{}, wrong{}, jump{}, boss{};
    bool loaded = false;

    static Sound Tone(float f0, float f1, float len, float noise, float vol) {
        const int rate = 44100, n = (int)(rate * len);
        std::vector<short> d(n);
        float phase = 0;
        for (int i = 0; i < n; i++) {
            float t = (float)i / n, f = f0 + (f1 - f0) * t;
            phase += 6.2831853f * f / rate;
            float v = std::sin(phase) * (1 - noise) + (Rand01() * 2 - 1) * noise;
            float env = std::min(1.0f, t * 40) * (1 - t) * (1 - t);
            d[i] = (short)(std::clamp(v * env * vol, -1.0f, 1.0f) * 32767);
        }
        Wave w = {};
        w.frameCount = (unsigned)n;
        w.sampleRate = rate;
        w.sampleSize = 16;
        w.channels = 1;
        w.data = d.data();
        return LoadSoundFromWave(w);
    }
    void Load() {
        if (loaded || std::getenv("SEGFAULT_MUTE")) return;
        if (!IsAudioDeviceReady()) InitAudioDevice();
        if (!IsAudioDeviceReady()) return;
        hit = Tone(520, 180, 0.12f, 0.45f, 0.5f);
        hurt = Tone(220, 90, 0.25f, 0.3f, 0.6f);
        right = Tone(660, 1320, 0.30f, 0, 0.45f);
        wrong = Tone(300, 140, 0.35f, 0.1f, 0.5f);
        jump = Tone(380, 760, 0.10f, 0, 0.3f);
        boss = Tone(90, 50, 0.6f, 0.5f, 0.7f);
        loaded = true;
    }
    void Play(Sound& s, float volume) {
        if (!loaded || volume <= 0) return;
        SetSoundVolume(s, volume);
        PlaySound(s);
    }
};

// --------------------------------------------------------------- settings ---
enum Action { ACT_LEFT, ACT_RIGHT, ACT_JUMP, ACT_ATTACK, ACT_USE, ACT_COUNT };
const char* kActionName[ACT_COUNT] = {"Move left", "Move right", "Jump", "Attack", "Use terminal"};
const int kDefaultKeys[ACT_COUNT] = {KEY_A, KEY_D, KEY_SPACE, KEY_J, KEY_E};

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
    int keys[ACT_COUNT] = {KEY_A, KEY_D, KEY_SPACE, KEY_J, KEY_E};
    int difficulty = 1;          // 0 easy (7 hearts), 1 normal (5), 2 hard (3)
    bool questionTimer = true;   // 25 s to answer
    float volume = 0.6f;
    int Hearts() const { return difficulty == 0 ? 7 : difficulty == 1 ? 5 : 3; }
};

// ------------------------------------------------------------- the world ---
struct Hero {
    float x = 200, y = kGround, vy = 0;
    int face = 1;
    float walk = 0, attack = -1, attackCd = 0, hurt = 0;
    int hearts = 5;
    bool grounded = true;
};

struct Goblin {
    float x, home;
    int face = -1;
    int hp = 3, maxHp = 3;
    float walk = 0, hurt = 0, dying = -1, touchCd = 0;
    bool alive = true;
};

struct Ring { float x, vx; float life; };

struct Popup { std::string text; float x, y, age; Color col; };

enum State { MENU, SETTINGS, STORY, CHAPTER, PLAY, QUIZ, TERMINAL, PAUSE, DEAD, VICTORY, CONFIRM_RESTART };

const float kLevelLen = 5200;

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
    {"Among the scales lies a ribbon from Ada's crown. Her trail leads down, into the Array Caverns...", 1},
    {"To be continued. Chapter 2, the Array Caverns, arrives in the next update.", 3},
};

// ================================================================= scene ===
class GameScene : public Scene {
public:
    const char* Title() const override { return "The Forgotten Pointer"; }
    bool AiAllowed() const override { return false; }

    void Enter(const std::string& param) override {
        LoadSave();
        state = MENU;
        menuSel = 0;
        t = 0;
        sfx.Load();
        // --shot hooks, to check each screen from the command line
        if (param == "settings") state = SETTINGS;
        else if (param == "story") StartStory(kIntro, 4, CHAPTER), panelIdx = 2, panelT = 100;
        else if (param == "play" || param == "quiz" || param == "boss" || param == "terminal") {
            ResetLevel();
            state = PLAY;
            if (param == "quiz") { hero.x = 900; AskQuestion(0, false); }
            if (param == "boss" || param == "terminal") { goblins.clear(); hero.x = kLevelLen - 600; Update(0.016f); bossT = 2; ringCd = 0; Update(0.5f); }
            if (param == "terminal") state = TERMINAL;
        }
    }

    void Frame(float dt) override {
        t += dt;
        switch (state) {
            case MENU: Menu(dt); break;
            case CONFIRM_RESTART: Menu(dt); ConfirmRestart(); break;
            case SETTINGS: SettingsScreen(); break;
            case STORY: Story(dt); break;
            case CHAPTER: Chapter(dt); break;
            case PLAY: Update(dt); Draw(); break;
            case QUIZ: Draw(); Quiz(dt); break;
            case TERMINAL: Terminal(); break;
            case PAUSE: Draw(); Pause(); break;
            case DEAD: Draw(); Dead(dt); break;
            case VICTORY: Victory(dt); break;
        }
    }

private:
    State state = MENU, settingsBack = MENU;
    float t = 0;
    Settings set;
    Sfx sfx;
    int chaptersDone = 0;   // saved progress
    int menuSel = 0;
    // settings screen
    int rebinding = -1, setSel = 0;
    // story
    const Panel* panels = nullptr;
    int panelCount = 0, panelIdx = 0;
    float panelT = 0;
    State afterStory = CHAPTER;
    float chapterT = 0;
    // play
    Hero hero;
    std::vector<Goblin> goblins;
    std::vector<Ring> rings;
    std::vector<Popup> popups;
    float cam = 0, shake = 0;
    bool arena = false, bossAwake = false;
    float arenaCam = 0, termX = 0;
    float bossHp = 1, bossHurt = 0, bossX = 0, bossT = 0, ringCd = 2, termCd = 0, bossDying = -1;
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
            else if (k == "key") { int a, v; in >> a >> v; if (a >= 0 && a < ACT_COUNT && v > 0) set.keys[a] = v; }
        }
        set.difficulty = std::clamp(set.difficulty, 0, 2);
        set.volume = Clamp01(set.volume);
    }
    void Save() const {
        std::ofstream out(SavePath());
        out << "chapters " << chaptersDone << "\n";
        out << "difficulty " << set.difficulty << "\n";
        out << "timer " << (set.questionTimer ? 1 : 0) << "\n";
        out << "volume " << set.volume << "\n";
        for (int a = 0; a < ACT_COUNT; a++) out << "key " << a << " " << set.keys[a] << "\n";
    }

    // ------------------------------------------------------------- input --
    bool Down(Action a) const {
        if (IsKeyDown(set.keys[a])) return true;
        if (a == ACT_LEFT) return IsKeyDown(KEY_LEFT);
        if (a == ACT_RIGHT) return IsKeyDown(KEY_RIGHT);
        return false;
    }
    bool Pressed(Action a) const {
        if (IsKeyPressed(set.keys[a])) return true;
        if (a == ACT_JUMP) return IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
        if (a == ACT_ATTACK) return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        return false;
    }

    // ---------------------------------------------------- shared drawing --
    // A menu entry in the game's style; returns true when chosen.
    bool Item(float cx, float y, const std::string& label, bool selected, bool enabled = true) {
        float w = 360, h = 54;
        Rectangle r = {cx - w / 2, y, w, h};
        bool hover = enabled && MouseIn(r);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return true;
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
        fp::DrawStackPlains(t * 40, t);
        // the hero, standing at the edge of the plains
        fp::DrawHero({ScreenRect().x + 220, kGround}, 1.3f, 1, 0, -1, t, false);
    }

    // -------------------------------------------------------------- menu --
    void Menu(float) {
        TitleBackdrop();
        Shade(0.35f);
        const float cx = W / 2;
        // the title
        float bob = std::sin(t * 1.3f) * 4;
        TextCenter("THE FORGOTTEN", {0, 110 + bob, W, 50}, 44, Hex(0xb9b0ff), F_HEAVY);
        TextCenter("POINTER", {0, 152 + bob, W, 100}, 92, WHITE, F_HEAVY);
        TextCenter("A SegFault story", {0, 260, W, 24}, 17, Hex(0xd8d3f5), F_MEDIUM);
        if (state != MENU) return;

        struct Entry { const char* label; bool enabled; int id; };
        std::vector<Entry> items;
        if (chaptersDone > 0) items.push_back({"Continue", true, 0});
        items.push_back({"New Game", true, 1});
        if (chaptersDone > 0) items.push_back({"Restart", true, 2});
        items.push_back({"Settings", true, 3});
        items.push_back({"Quit to desktop", true, 4});
        menuSel = std::clamp(menuSel, 0, (int)items.size() - 1);
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) menuSel = (menuSel + 1) % items.size();
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) menuSel = (menuSel + (int)items.size() - 1) % items.size();
        int chosen = -1;
        for (size_t i = 0; i < items.size(); i++) {
            float y = 340 + i * 64;
            Rectangle r = {cx - 180, y, 360, 54};
            if (MouseIn(r)) menuSel = (int)i;
            if (Item(cx, y, items[i].label, (int)i == menuSel, items[i].enabled)) chosen = items[i].id;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) chosen = items[menuSel].id;
        TextCenter("Arrows / mouse to choose, Enter to start", {0, H - 46, W, 20}, 13, Hex(0x8f88b8), F_MEDIUM);

        switch (chosen) {
            case 0: StartChapter(); break;   // there is only chapter 1 so far
            case 1:
                if (chaptersDone > 0) { state = CONFIRM_RESTART; break; }
                StartStory(kIntro, 4, CHAPTER);
                break;
            case 2: state = CONFIRM_RESTART; break;
            case 3: settingsBack = MENU; state = SETTINGS; rebinding = -1; break;
            case 4: Leave(); break;
            default: break;
        }
    }

    void ConfirmRestart() {
        Shade(0.6f);
        Rectangle box = {W / 2 - 260, H / 2 - 110, 520, 220};
        DrawRectangleRounded(box, 0.12f, 12, Hex(0x1b1736));
        DrawRectangleRoundedLinesEx(box, 0.12f, 12, 2, Hex(0x8b7bff));
        TextCenter("Start again from the beginning?", {box.x, box.y + 34, box.width, 30}, 22, WHITE, F_BOLD);
        TextCenter("Your progress through the lands is lost.", {box.x, box.y + 74, box.width, 22}, 15, Hex(0xb9b3d9), F_MEDIUM);
        bool yes = Item(W / 2 - 120, box.y + 130, "Yes, restart", false) || IsKeyPressed(KEY_Y);
        bool no = Item(W / 2 + 120, box.y + 130, "Cancel", false) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_N);
        if (yes) { chaptersDone = 0; Save(); StartStory(kIntro, 4, CHAPTER); }
        else if (no) state = MENU;
    }

    void Leave() {
        if (app.hasReturn) app.Go(app.returnScene, app.returnParam);
        else app.Go(SceneId::Desktop, app.user.empty() ? "login" : "home");
    }

    // ---------------------------------------------------------- settings --
    void SettingsScreen() {
        TitleBackdrop();
        Shade(0.72f);
        TextCenter("Settings", {0, 70, W, 50}, 40, WHITE, F_HEAVY);
        const float x0 = W / 2 - 300, w = 600;
        float y = 150;
        // controls
        Text("CONTROLS", x0, y, 14, Hex(0x8f88b8), F_BOLD);
        y += 28;
        for (int a = 0; a < ACT_COUNT; a++) {
            Rectangle row = {x0, y, w, 46};
            bool hover = MouseIn(row);
            DrawRectangleRounded(row, 0.3f, 8, Fade(Hex(0x2a2550), hover ? 0.9f : 0.6f));
            Text(kActionName[a], row.x + 18, row.y + 13, 17, Hex(0xe6e1ff), F_MEDIUM);
            std::string k = rebinding == a ? "press a key..." : KeyName(set.keys[a]);
            if (a == ACT_LEFT) k += rebinding == a ? "" : "  /  Left";
            if (a == ACT_RIGHT) k += rebinding == a ? "" : "  /  Right";
            if (a == ACT_JUMP) k += rebinding == a ? "" : "  /  Up";
            if (a == ACT_ATTACK) k += rebinding == a ? "" : "  /  Mouse";
            TextRight(k, row.x + row.width - 18, row.y + 13, 17, rebinding == a ? Hex(0xff9e4a) : Hex(0xb9b0ff), F_BOLD);
            if (hover) WantHand();
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && rebinding < 0) rebinding = a;
            y += 52;
        }
        if (rebinding >= 0) {
            int k = GetKeyPressed();
            if (k == KEY_ESCAPE) rebinding = -1;
            else if (k > 0) { set.keys[rebinding] = k; rebinding = -1; Save(); }
        }
        y += 14;
        Text("GAME", x0, y, 14, Hex(0x8f88b8), F_BOLD);
        y += 28;
        // difficulty
        const char* diff[3] = {"Easy  (7 hearts)", "Normal  (5 hearts)", "Hard  (3 hearts)"};
        if (Cycle({x0, y, w, 46}, "Difficulty", diff[set.difficulty])) { set.difficulty = (set.difficulty + 1) % 3; Save(); }
        y += 52;
        if (Cycle({x0, y, w, 46}, "Question timer", set.questionTimer ? "On  (25 s)" : "Off")) { set.questionTimer = !set.questionTimer; Save(); }
        y += 52;
        // volume: click the bar
        Rectangle row = {x0, y, w, 46};
        DrawRectangleRounded(row, 0.3f, 8, Fade(Hex(0x2a2550), 0.6f));
        Text("Sound", row.x + 18, row.y + 13, 17, Hex(0xe6e1ff), F_MEDIUM);
        Rectangle bar = {row.x + w - 260, row.y + 19, 200, 8};
        DrawRectangleRounded(bar, 1, 8, Hex(0x3b3470));
        DrawRectangleRounded({bar.x, bar.y, bar.width * set.volume, bar.height}, 1, 8, Hex(0x8b7bff));
        DrawCircleV({bar.x + bar.width * set.volume, bar.y + 4}, 9, WHITE);
        TextRight(std::to_string((int)std::lround(set.volume * 100)) + "%", row.x + w - 18, row.y + 13, 16, Hex(0xb9b0ff), F_BOLD);
        Rectangle hit = {bar.x - 10, row.y, bar.width + 20, row.height};
        if (MouseIn(hit)) WantHand();
        if (MouseIn(hit) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) set.volume = Clamp01((Mouse().x - bar.x) / bar.width);
        if (MouseIn(hit) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) { Save(); sfx.Play(sfx.right, set.volume); }
        y += 70;
        if (Item(W / 2, y, "Back", false) || (rebinding < 0 && IsKeyPressed(KEY_ESCAPE))) { Save(); state = settingsBack; }
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
    void StartStory(const Panel* p, int n, State after) {
        panels = p;
        panelCount = n;
        panelIdx = 0;
        panelT = 0;
        afterStory = after;
        state = STORY;
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
        if (afterStory == CHAPTER) StartChapter();
        else state = afterStory;
    }

    // ------------------------------------------------------- chapter card --
    void StartChapter() {
        state = CHAPTER;
        chapterT = 0;
        ResetLevel();
    }

    void Chapter(float dt) {
        chapterT += dt;
        Draw();
        float a = chapterT < 2.2f ? 1 : 1 - Clamp01((chapterT - 2.2f) / 0.6f);
        Shade(0.75f * a);
        TextCenter("CHAPTER 1", {0, 280, W, 30}, 20, Fade(Hex(0xb9b0ff), a), F_BOLD);
        TextCenter("Stack Plains", {0, 312, W, 70}, 60, Fade(WHITE, a), F_HEAVY);
        TextCenter("Loops and numbers", {0, 392, W, 26}, 18, Fade(Hex(0xd8d3f5), a), F_MEDIUM);
        if (chapterT > 2.8f || ((IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) && chapterT > 0.3f)) state = PLAY;
    }

    // ------------------------------------------------------------ the level --
    void ResetLevel() {
        hero = Hero();
        hero.hearts = set.Hearts();
        goblins.clear();
        const float xs[] = {1000, 1500, 2050, 2550, 3100, 3650};
        for (float x : xs) {
            Goblin g;
            g.x = g.home = x;
            g.hp = g.maxHp = GetRandomValue(2, 3);
            goblins.push_back(g);
        }
        rings.clear();
        popups.clear();
        arena = bossAwake = false;
        bossHp = 1;
        bossDying = -1;
        bossHurt = 0;
        bestPassed = 0;
        termMsg.clear();
        cam = 0;
        // questions about loops and numbers, shuffled; each one once before any repeats
        questionPool.clear();
        for (const Question& qq : Questions())
            if (qq.section == S_NUMBERS || qq.section == S_LOOPS) questionPool.push_back(qq.id);
        Shuffle(questionPool);
        poolNext = 0;
        // the boss's problem: a medium one about loops or numbers
        std::vector<const Problem*> pick;
        for (const Problem& p : Problems())
            if ((p.section == S_NUMBERS || p.section == S_LOOPS) && p.difficulty == 1) pick.push_back(&p);
        bossProblem = pick.empty() ? nullptr : pick[GetRandomValue(0, (int)pick.size() - 1)];
        if (bossProblem) ws.Open(bossProblem, "", true);
    }

    template <class T> static void Shuffle(std::vector<T>& v) {
        for (int i = (int)v.size() - 1; i > 0; i--) std::swap(v[i], v[GetRandomValue(0, i)]);
    }

    void Pop(const std::string& s, float x, float y, Color c) { popups.push_back({s, x, y, 0, c}); }

    void HurtHero(int fromDir) {
        if (hero.hurt > 0) return;
        hero.hearts--;
        hero.hurt = 1.0f;
        hero.vy = -420;
        hero.grounded = false;
        hero.x += fromDir * 60;
        shake = 0.3f;
        sfx.Play(sfx.hurt, set.volume);
        Pop("-1", hero.x, hero.y - 205, Hex(0xff5c7a));
        if (hero.hearts <= 0) { state = DEAD; deadT = 0; }
    }

    void Update(float dt) {
        if (IsKeyPressed(KEY_ESCAPE)) { state = PAUSE; menuSel = 0; return; }
        Rectangle scr = ScreenRect();
        const float L = scr.x, vw = scr.width;
        // ---- the hero
        float move = (Down(ACT_RIGHT) ? 1.0f : 0) - (Down(ACT_LEFT) ? 1.0f : 0);
        if (move != 0) hero.face = move > 0 ? 1 : -1;
        hero.x += move * 340 * dt;
        hero.walk = move != 0 && hero.grounded ? hero.walk + dt * 11 : 0;
        if (Pressed(ACT_JUMP) && hero.grounded) { hero.vy = -820; hero.grounded = false; sfx.Play(sfx.jump, set.volume * 0.6f); }
        hero.vy += 2300 * dt;
        hero.y += hero.vy * dt;
        if (hero.y >= kGround) { hero.y = kGround; hero.vy = 0; hero.grounded = true; }
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
            for (size_t i = 0; i < goblins.size(); i++) {
                Goblin& g = goblins[i];
                float dx = (g.x - hero.x) * hero.face;
                if (!g.alive || g.dying >= 0 || dx < -20 || dx > 130 || std::fabs(hero.y - kGround) > 60) continue;
                Strike((int)i);
                break;
            }
        }

        // ---- goblins: patrol, chase when close, hurt on touch
        for (Goblin& g : goblins) {
            if (!g.alive) continue;
            g.hurt = std::max(0.0f, g.hurt - dt * 3);
            g.touchCd = std::max(0.0f, g.touchCd - dt);
            if (g.dying >= 0) { g.dying += dt; if (g.dying > 0.6f) g.alive = false; continue; }
            float dx = hero.x - g.x;
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
            bossX = arenaCam + L + vw * 0.68f;
            bossT = 0;
            ringCd = 2.5f;
            termCd = 0;
            sfx.Play(sfx.boss, set.volume);
            shake = 0.6f;
            Pop("The Infinite Loop Serpent awakens!", hero.x, hero.y - 220, Hex(0x9ff5ea));
            bossAwake = true;
        }
        if (bossAwake && bossDying < 0) {
            bossT += dt;
            bossHurt = std::max(0.0f, bossHurt - dt * 2);
            // it drifts back and forth over the right of the arena
            float span = vw * 0.22f;
            bossX = arenaCam + L + vw * 0.66f + std::sin(bossT * 0.7f) * span;
            // spits loop rings that roll along the ground: jump over them
            ringCd -= dt;
            if (ringCd <= 0) {
                rings.push_back({bossX - 120, -(300.0f + GetRandomValue(0, 140)), 6});
                ringCd = 1.6f + Rand01() * 1.2f;
            }
            // touching the coil hurts
            Vector2 bc = BossCenter();
            float ddx = hero.x - bc.x, ddy = (hero.y - 70) - bc.y;
            if (ddx * ddx + ddy * ddy < 170.0f * 170.0f) HurtHero(ddx > 0 ? 1 : -1);
            termCd = std::max(0.0f, termCd - dt);
            bool near = std::fabs(hero.x - termX) < 90;
            if (near && termCd <= 0 && Pressed(ACT_USE) && bossProblem) { state = TERMINAL; wsRunning = false; termMsg.clear(); return; }
        }
        if (bossDying >= 0) {
            bossDying += dt;
            if (bossDying > 2.4f) { Win(); return; }
        }
        for (Ring& r : rings) {
            r.x += r.vx * dt;
            r.life -= dt;
            if (std::fabs(r.x - hero.x) < 38 && hero.y > kGround - 50 && r.life > 0) { r.life = 0; HurtHero(r.vx > 0 ? 1 : -1); if (state != PLAY) return; }
        }
        rings.erase(std::remove_if(rings.begin(), rings.end(), [&](const Ring& r) { return r.life <= 0 || r.x < arenaCam + L - 100; }), rings.end());

        // ---- camera
        float target = hero.x - (L + vw * 0.4f);
        target = std::clamp(target, -L, kLevelLen - vw - L);
        if (arena) target = arenaCam;
        cam += (target - cam) * std::min(1.0f, dt * 6);
        shake = std::max(0.0f, shake - dt);
        for (Popup& p : popups) p.age += dt;
        popups.erase(std::remove_if(popups.begin(), popups.end(), [](const Popup& p) { return p.age > 1.4f; }), popups.end());
    }

    Vector2 BossCenter() const { return {bossX, 330 + std::sin(bossT * 1.3f) * 30}; }

    // A sword hit on a goblin: sometimes (and always on the last hit) a question.
    void Strike(int i) {
        Goblin& g = goblins[i];
        sfx.Play(sfx.hit, set.volume);
        g.hurt = 1;
        g.x += hero.face * 26;
        bool last = g.hp <= 1;
        if (last || Rand01() < 0.4f) { AskQuestion(i, last); return; }
        g.hp--;
        Pop("hit", g.x, kGround - 120, Hex(0xd8d3f5));
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
        Text("Answer right and the goblin falls. Wrong, and it strikes back.", box.x + 30, box.y + 46, 13.5f, Hex(0x8f88b8), F_MEDIUM);
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
        sfx.Play(quizRight ? sfx.right : sfx.wrong, set.volume);
    }

    void ResolveQuiz() {
        state = PLAY;
        Goblin& g = goblins[quizTarget];
        if (quizRight) {
            g.hp = quizFinal ? 0 : std::max(0, g.hp - 2);   // a critical hit
            Pop(quizFinal ? "Finished!" : "CRITICAL!", g.x, kGround - 140, Hex(0xffc14d));
            if (g.hp <= 0) g.dying = 0;
            shake = 0.15f;
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
        Text("Every test your code passes hits the Serpent. A compile error lets it strike you.", top.x + 140, top.y + 15, 14, Hex(0xb9b3d9), F_MEDIUM);
        // the Serpent's health
        Rectangle hb = {top.x + top.width - 330, top.y + 17, 200, 12};
        DrawRectangleRounded(hb, 1, 8, Hex(0x2a2550));
        DrawRectangleRounded({hb.x, hb.y, hb.width * bossHp, hb.height}, 1, 8, Hex(0x3fd1c7));
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
                termMsg = r.status == RunReport::CompileError ? "It doesn't compile - the Serpent strikes!" : "Your program crashed - the Serpent strikes!";
                hero.hurt = 0;
                HurtHero(-1);
                if (state == DEAD) return;
            } else if (r.total > 0 && r.passed > bestPassed) {
                int gained = r.passed - bestPassed;
                bestPassed = r.passed;
                bossHp = 1 - (float)bestPassed / r.total;
                bossHurt = 1;
                sfx.Play(sfx.hit, set.volume);
                termMsg = std::to_string(gained) + (gained == 1 ? " new test passes" : " new tests pass") + " - a direct hit!";
                if (bestPassed >= r.total) { bossDying = 0; state = PLAY; sfx.Play(sfx.boss, set.volume); shake = 1.0f; return; }
            } else termMsg = "No new test passes yet. Look at the failing case and try again.";
        }
        if (bh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { state = PLAY; termCd = 1.5f; }
        else if (IsKeyPressed(KEY_ESCAPE) && !AnyTextFocused()) { state = PLAY; termCd = 1.5f; }
    }

    void Win() {
        chaptersDone = std::max(chaptersDone, 1);
        Save();
        state = VICTORY;
        deadT = 0;
    }

    void Victory(float dt) {
        deadT += dt;
        Draw();
        Shade(Clamp01(deadT / 0.8f) * 0.8f);
        TextCenter("CHAPTER 1 CLEARED", {0, 300, W, 60}, 52, Fade(Hex(0x48e5a0), Clamp01(deadT)), F_HEAVY);
        TextCenter("The Infinite Loop Serpent is defeated.", {0, 370, W, 26}, 19, Fade(Hex(0xd8d3f5), Clamp01(deadT)), F_MEDIUM);
        if (deadT > 1.2f && (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || deadT > 4)) StartStory(kAfterChapter1, 3, MENU);
    }

    // --------------------------------------------------- pause and death --
    void Pause() {
        Shade(0.6f);
        TextCenter("Paused", {0, 200, W, 60}, 48, WHITE, F_HEAVY);
        const char* items[3] = {"Resume", "Settings", "Main menu"};
        if (IsKeyPressed(KEY_DOWN)) menuSel = (menuSel + 1) % 3;
        if (IsKeyPressed(KEY_UP)) menuSel = (menuSel + 2) % 3;
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
        if (chosen == 2) { state = MENU; menuSel = 0; }
    }

    void Dead(float dt) {
        deadT += dt;
        Shade(Clamp01(deadT / 0.6f) * 0.85f);
        float a = Clamp01(deadT / 0.6f);
        TextCenter("KERNEL PANIC", {0, 250, W, 70}, 64, Fade(Hex(0xff3d5a), a), F_HEAVY);
        TextCenter("The Debugger has fallen. The bugs win this round.", {0, 330, W, 26}, 19, Fade(Hex(0xd8d3f5), a), F_MEDIUM);
        if (deadT < 0.8f) return;
        if (Item(W / 2, 400, "Try again", menuSel == 0)) StartChapter();
        if (Item(W / 2, 464, "Main menu", menuSel == 1)) { state = MENU; menuSel = 0; }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_UP)) menuSel = 1 - std::clamp(menuSel, 0, 1);
        if (IsKeyPressed(KEY_ENTER)) { if (menuSel == 0) StartChapter(); else { state = MENU; menuSel = 0; } }
    }

    // ------------------------------------------------------------- drawing --
    void Draw() {
        Rectangle scr = ScreenRect();
        float sx = shake > 0 ? std::sin(t * 90) * shake * 10 : 0;
        float c = cam - sx;
        fp::DrawStackPlains(c, t);
        auto X = [&](float wx) { return wx - c; };
        // signposts along the way
        for (float wx = 600; wx < kLevelLen - 1200; wx += 1400) {
            float x = X(wx);
            if (x < scr.x - 100 || x > scr.x + scr.width + 100) continue;
            DrawRectangle((int)x - 4, (int)kGround - 90, 8, 90, Hex(0x7a4a2a));
            DrawRectangleRounded({x - 60, kGround - 120, 120, 40}, 0.2f, 6, Hex(0xe8d3a8));
            TextCenter(TextFormat("0x%04X", (int)wx), {x - 60, kGround - 112, 120, 24}, 16, Hex(0x5a3a1a), F_MONOBOLD);
        }
        // the arena: the terminal and the boss
        if (arena) {
            fp::DrawTerminalPillar({X(termX), kGround}, t, termCd <= 0 && bossDying < 0, std::fabs(hero.x - termX) < 90);
            if (bossDying < 0 && std::fabs(hero.x - termX) < 90 && termCd <= 0)
                TextCenter("Press " + KeyName(set.keys[ACT_USE]) + " to write code", {X(termX) - 150, kGround - 250, 300, 22}, 16, Hex(0x48e5ff), F_BOLD);
        }
        if (bossAwake) {
            Vector2 bc = BossCenter();
            float alpha = bossDying >= 0 ? 1 - Clamp01(bossDying / 2.2f) : 1;
            float radius = 130 + (bossDying >= 0 ? bossDying * 40 : 0);
            fp::DrawSerpent({X(bc.x), bc.y}, radius, t, bossHurt, alpha);
        }
        for (const Ring& r : rings) fp::DrawLoopRing({X(r.x), kGround - 26}, 26, t);
        // goblins
        for (const Goblin& g : goblins) {
            if (!g.alive) continue;
            float a = g.dying >= 0 ? 1 - Clamp01(g.dying / 0.6f) : 1;
            fp::DrawGoblin({X(g.x), kGround + (g.dying >= 0 ? g.dying * 30 : 0)}, 1.0f, g.face, g.walk, g.hurt, a);
            if (g.dying < 0 && g.hp < g.maxHp) {   // a small health bar once hit
                Rectangle hb = {X(g.x) - 24, kGround - 140, 48, 6};
                DrawRectangleRounded(hb, 1, 6, Hex(0x2a2550));
                DrawRectangleRounded({hb.x, hb.y, hb.width * g.hp / g.maxHp, hb.height}, 1, 6, Hex(0x6fd15a));
            }
        }
        // the hero (blinks while hurt)
        bool blink = hero.hurt > 0 && std::fmod(hero.hurt, 0.16f) < 0.08f;
        fp::DrawHero({X(hero.x), hero.y}, 1.0f, hero.face, hero.walk, hero.attack, t, blink);
        for (const Popup& p : popups)
            TextCenter(p.text, {X(p.x) - 200, p.y - p.age * 50, 400, 30}, 22, Fade(p.col, 1 - p.age / 1.4f), F_HEAVY);
        Hud();
    }

    void Hud() {
        Rectangle scr = ScreenRect();
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
        for (const Goblin& g : goblins) left += g.alive && g.dying < 0;
        std::string info = arena ? "Defeat the Infinite Loop Serpent" : (left > 0 ? std::to_string(left) + " goblins ahead" : "Go right, to the guardian");
        TextRight("Stack Plains", scr.x + scr.width - 24, 18, 18, WHITE, F_HEAVY);
        TextRight(info, scr.x + scr.width - 24, 42, 13.5f, Hex(0xd8d3f5), F_MEDIUM);
        if (bossAwake && bossDying < 0) {
            Rectangle hb = {W / 2 - 220, 22, 440, 16};
            DrawRectangleRounded(hb, 1, 8, Hex(0x2a2550));
            DrawRectangleRounded({hb.x, hb.y, hb.width * bossHp, hb.height}, 1, 8, Hex(0x3fd1c7));
            TextCenter("Infinite Loop Serpent", {hb.x, hb.y + 20, hb.width, 18}, 14, Hex(0x9ff5ea), F_BOLD);
        }
        if (!arena && hero.x < 800 && state == PLAY)
            TextCenter(KeyName(set.keys[ACT_LEFT]) + "/" + KeyName(set.keys[ACT_RIGHT]) + " to move, " + KeyName(set.keys[ACT_JUMP]) +
                           " to jump, " + KeyName(set.keys[ACT_ATTACK]) + " to attack. Esc pauses.",
                       {0, H - 40, W, 20}, 14, Hex(0xd8d3f5), F_MEDIUM);
    }
};

} // namespace

Scene* CreateGameScene() { return new GameScene(); }
