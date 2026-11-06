// ============================================================================
// content.h  -  All learning content as data: lessons, coding problems, quiz
// questions and the memory-visualizer programs.
// ============================================================================
#pragma once
#include "app/app.h"
#include <functional>
#include <string>
#include <vector>

// ------------------------------------------------------------- lessons -----
enum class BlockType {
    Text,       // rich text
    Code,       // a code sample
    Walk,       // code you step through line by line, with an explanation per step
    Table,      // rows separated by '\n', cells by '|', first row = header
    Tip,        // green callout
    Warn,       // red callout ("common mistake")
    Demo,       // button that opens an interactive demo (visualizer, list lab, grid)
    Practice,   // links to coding problems (ids separated by ',')
    Check       // a quick multiple-choice check (first option = correct)
};

struct WalkStep { int line; std::string text; };   // line is 0-based

struct Block {
    BlockType type = BlockType::Text;
    std::string text;
    std::vector<WalkStep> steps;
    SceneId scene = SceneId::Home;
    std::string param;
    std::vector<std::string> options;
    std::string explain;
};

struct Lesson {
    std::string id;
    int section;
    std::string title;
    std::string summary;
    std::vector<Block> blocks;
};
const std::vector<Lesson>& Lessons();

// ------------------------------------------------------------ problems -----
struct Example { std::string input, output, explain; };

struct Problem {
    std::string id;
    std::string title;
    int section = 0;
    int difficulty = 0;              // 0 easy, 1 medium, 2 hard
    std::string statement;           // rich text
    std::vector<Example> examples;
    std::string constraints;
    std::string starter;             // code the editor starts with
    std::string solution;            // reference solution (same shape as starter)
    std::string tests;               // harness body: PQ_TOTAL(n); then one check per case
    std::vector<std::string> hints;
    bool cppOnly = false;            // needs a C++ compiler (references, classes, new)
    std::vector<std::string> forbid; // exam rules, e.g. "[" (no indexing) or "if" (no conditionals)
    std::string ruleText;            // shown when a forbidden token is used
    int paper = 1;                   // mock exam paper (1: sections up to Strings, 2: pointers onwards)
    int exampleCases = 3;            // first N test cases are the visible examples
};
const std::vector<Problem>& Problems();
const Problem* ProblemById(const std::string& id);
const char* DifficultyName(int d);
Color DifficultyColor(int d);

// ----------------------------------------------------------- questions -----
struct Question {
    int id;
    int section;
    std::string prompt;
    std::string code;
    std::vector<std::string> options;    // first option listed = correct one
    std::string explain;
};
const std::vector<Question>& Questions();
const Question* QuestionById(int id);

// ---------------------------------------------------- memory visualizer -----
enum class Region { Stack, Heap };
enum class CellState { Alive, OutOfScope, Freed };

struct MemCell {
    int addr = 0;
    std::string name;
    std::string type;
    std::string value;
    int pointsTo = -1;
    Region region = Region::Stack;
    CellState state = CellState::Alive;
    std::string frame;
    bool changed = false;
};

struct MemState {
    std::vector<MemCell> cells;
    std::vector<std::string> output;

    MemCell* Find(const std::string& name);
    MemCell* At(int addr);
    void Decl(const std::string& frame, const std::string& name, const std::string& type, int addr,
              const std::string& value = "?");
    void Set(const std::string& name, const std::string& value);
    void Point(const std::string& name, int addr);
    void PointNull(const std::string& name);
    void Deref(const std::string& ptrName, const std::string& value);
    void New(int addr, const std::string& type, const std::string& value);
    void Free(int addr);
    void EndScope(const std::string& name);
    void Print(const std::string& text) { output.push_back(text); }
};

struct MemStep {
    int line;
    std::string note;
    std::function<void(MemState&)> apply;
    std::string ask;
    std::vector<std::string> choices;
};

struct MemProgram {
    std::string id;
    int topic;
    std::string title;
    std::string blurb;
    std::vector<std::string> code;
    std::vector<MemStep> steps;
};
const std::vector<MemProgram>& MemPrograms();
