// ============================================================================
// app.h  -  Shared app state: navigation, sections, progress, source snippets.
// ============================================================================
#pragma once
#include "raylib.h"
#include <map>
#include <set>
#include <string>
#include <vector>

// ----------------------------------------------------------- sections -----
enum Section {
    S_NUMBERS, S_LOOPS, S_ARRAYS, S_GRID, S_STRINGS,
    S_POINTERS, S_PTRARITH, S_MEMORY, S_LISTS, S_COUNT
};
const char* SectionName(int s);
const char* SectionShort(int s);     // one-line description
Color SectionColor(int s);
// old names kept for the interactive demo scenes
inline const char* TopicName(int s) { return SectionName(s); }
inline Color TopicColor(int s) { return SectionColor(s); }

// ------------------------------------------------------------- scenes -----
enum class SceneId { Home, Study, Practice, Mock, Quiz, Visualizer, ListLab, Grid, Count };

class Scene {
public:
    virtual ~Scene() {}
    virtual const char* Title() const = 0;
    virtual void Enter(const std::string& param) { (void)param; }
    virtual void Frame(float dt) = 0;     // update + draw (immediate mode)
};

// ----------------------------------------------------------- progress -----
struct Progress {
    int xp = 0;
    std::set<std::string> done;          // "lesson:x", "solved:x", "tried:x", "q:12", "prog:x", ...
    std::set<int> mistakes;              // quiz question ids waiting for review
    std::map<std::string, int> best;     // best scores ("mock:1", "quiz:streak", ...)
    int quizAnswered = 0, quizCorrect = 0;
    std::string path;

    bool Has(const std::string& key) const { return done.count(key) > 0; }
    bool Mark(const std::string& key);   // true the first time
    void AddXP(int amount, const std::string& why);
    void SetBest(const std::string& key, int value);
    int Best(const std::string& key, int def = -1) const;
    int Level() const { return xp / 200 + 1; }
    float LevelProgress() const { return (xp % 200) / 200.0f; }
    void Load(const std::string& file);
    void Save() const;
    void Reset();
};

// ---------------------------------------------------------------- app -----
struct App {
    SceneId current = SceneId::Home;
    std::string currentParam;
    SceneId pending = SceneId::Home;
    std::string pendingParam;
    bool switchRequested = false;
    // "Back" target for scenes opened from another one (e.g. a demo opened from a lesson)
    bool hasReturn = false;
    SceneId returnScene = SceneId::Home;
    std::string returnParam;

    std::string resourceDir;
    std::string dataDir;                  // progress + saved code
    Progress progress;
    std::string consoleCapture;           // text printed by exam functions

    void Go(SceneId id, const std::string& param = "") {
        pending = id; pendingParam = param; switchRequested = true; hasReturn = false;
    }
    // Opens a scene and remembers where to come back to.
    void Open(SceneId id, const std::string& param, const std::string& backParam) {
        returnScene = current; returnParam = backParam;
        pending = id; pendingParam = param; switchRequested = true; hasReturn = true;
    }
};
extern App app;

// Captures examPrintf output into app.consoleCapture while alive.
struct CaptureOutput {
    CaptureOutput();
    ~CaptureOutput();
};

float SectionMastery(int section);   // 0..1 (lessons + solved problems + quiz)

// Saved student code per problem (data/code/<id>.cpp)
std::string LoadSavedCode(const std::string& key);
void SaveCode(const std::string& key, const std::string& code);

// ------------------------------------------------------ source snippets ----
namespace sources {
    void Load(const std::string& dir);
    bool Has(const std::string& name);
    const std::vector<std::string>& Get(const std::string& name);
    std::string Text(const std::string& name);   // joined with '\n'
    int FindLine(const std::string& name, const std::string& containing, int startAt = 0);
}

// Scene factories (scenes/*.cpp)
Scene* CreateHomeScene();
Scene* CreateStudyScene();
Scene* CreatePracticeScene();
Scene* CreateMockScene();
Scene* CreateQuizScene();
Scene* CreateVisualizerScene();
Scene* CreateListLabScene();
Scene* CreateGridScene();
