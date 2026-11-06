// ============================================================================
// workspace.h  -  The LeetCode-style problem screen shared by Practice and
// the Mock Exam: description on the left, code editor on the right, and a
// console with the test results underneath.
// ============================================================================
#pragma once
#include "app/editor.h"
#include "app/runner.h"
#include "app/ui.h"
#include "content/content.h"
#include <map>
#include <string>

class Workspace {
public:
    // saveKey: where the code is saved ("" = keep it in memory only)
    void Open(const Problem* p, const std::string& saveKey, bool examMode);
    void Frame(Rectangle area);
    void Run(bool submit);

    const Problem* problem = nullptr;
    std::string Code() const { return editor.Text(); }
    bool Running() const { return job.Busy(); }
    bool HasReport() const { return hasReport; }
    const RunReport& Report() const { return report; }
    bool LastWasSubmit() const { return lastSubmit; }
    // true for one frame after a submit that passed every test
    bool TakeSolved() { bool s = solvedEvent; solvedEvent = false; return s; }
    void SaveNow();

private:
    CodeEditor editor;
    RunJob job;
    RunReport report;
    bool hasReport = false, lastSubmit = false, pendingSubmit = false, solvedEvent = false;
    bool examMode = false;
    std::string saveKey;
    int leftTab = 0, consoleTab = 0, selCase = 0, hintsShown = 0;
    bool solutionUnlocked = false;
    double dirtySince = -1;
    ui::Scroll descScroll, consoleScroll, solScroll;

    void Description(Rectangle r);
    void Toolbar(Rectangle r);
    void Console(Rectangle r);
    float DescContent(Rectangle r, bool draw);
};

// Small helpers shared by the scenes
void DrawSpinner(Vector2 c, float radius, Color col);
Color StatusColor(RunReport::Status s);
void CompilerChip(Rectangle r);   // shows (and cycles) the active compiler
