#include "app.h"
#include "ui.h"
#include "content/content.h"
#include "exam_output.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

App app;

// ============================================================ sections =====
const char* SectionName(int s) {
    static const char* names[S_COUNT] = {"Numbers & Digits", "Loops & Patterns", "Arrays", "2D Arrays",
                                         "Strings & Parsing", "Pointers", "Pointer Arithmetic",
                                         "Memory Management", "Linked Lists"};
    return (s >= 0 && s < S_COUNT) ? names[s] : "Mixed";
}
const char* SectionShort(int s) {
    static const char* d[S_COUNT] = {
        "% and /, digits, leap years, primes, binary",
        "nested loops and printing shapes",
        "search, count, insert, delete, reverse, rotate",
        "rows and columns, clusters and gravity",
        "char arrays, '\\0', reading C code as text",
        "addresses, & and *, pass by pointer, int**",
        "ptr + 1, arrays as pointers, string functions",
        "stack vs heap, new/delete, leaks, malloc/free",
        "nodes, head, singly and doubly linked lists"};
    return (s >= 0 && s < S_COUNT) ? d[s] : "";
}
Color SectionColor(int s) {
    static const Color c[S_COUNT] = {{45, 212, 191, 255}, {94, 234, 212, 255}, {56, 189, 248, 255},
                                     {163, 230, 53, 255}, {250, 204, 21, 255}, {251, 146, 60, 255},
                                     {249, 115, 22, 255}, {192, 132, 252, 255}, {244, 114, 182, 255}};
    return (s >= 0 && s < S_COUNT) ? c[s] : ui::col::accent;
}

// ============================================================ progress =====
bool Progress::Mark(const std::string& key) {
    if (done.count(key)) return false;
    done.insert(key);
    Save();
    return true;
}

void Progress::AddXP(int amount, const std::string& why) {
    int before = Level();
    xp += amount;
    ui::Toast("+" + std::to_string(amount) + " XP   " + why, ui::col::accent2);
    if (Level() > before) ui::Toast("Level up!  You are level " + std::to_string(Level()), ui::col::warn);
    Save();
}

void Progress::SetBest(const std::string& key, int value) {
    auto it = best.find(key);
    if (it == best.end() || value > it->second) { best[key] = value; Save(); }
}

int Progress::Best(const std::string& key, int def) const {
    auto it = best.find(key);
    return it == best.end() ? def : it->second;
}

void Progress::Load(const std::string& file) {
    path = file;
    std::ifstream in(file);
    std::string key;
    while (in >> key) {
        if (key == "xp") in >> xp;
        else if (key == "done") { std::string v; in >> v; done.insert(v); }
        else if (key == "mistake") { int id; in >> id; mistakes.insert(id); }
        else if (key == "best") { std::string k; int v; in >> k >> v; best[k] = v; }
        else if (key == "answered") in >> quizAnswered;
        else if (key == "correct") in >> quizCorrect;
        else { std::string rest; std::getline(in, rest); }
    }
}

void Progress::Save() const {
    if (path.empty()) return;
    std::ofstream out(path);
    if (!out) return;
    out << "xp " << xp << "\nanswered " << quizAnswered << "\ncorrect " << quizCorrect << "\n";
    for (auto& d : done) out << "done " << d << "\n";
    for (int m : mistakes) out << "mistake " << m << "\n";
    for (auto& b : best) out << "best " << b.first << " " << b.second << "\n";
}

void Progress::Reset() {
    std::string p = path;
    *this = Progress();
    path = p;
    Save();
}

float SectionMastery(int section) {
    int total = 0, got = 0;
    for (const Lesson& l : Lessons()) if (l.section == section) { total += 2; got += app.progress.Has("lesson:" + l.id) ? 2 : 0; }
    for (const Problem& p : Problems()) if (p.section == section) { total += 3; got += app.progress.Has("solved:" + p.id) ? 3 : 0; }
    for (const Question& q : Questions()) if (q.section == section) { total++; got += app.progress.Has("q:" + std::to_string(q.id)); }
    return total ? (float)got / total : 0;
}

// ========================================================= saved code ======
static std::string CodePath(const std::string& key) {
    std::string safe;
    for (char c : key) safe += (std::isalnum((unsigned char)c) || c == '_' || c == '-') ? c : '_';
    return app.dataDir + "segfault_code/" + safe + ".cpp";
}

std::string LoadSavedCode(const std::string& key) {
    if (app.dataDir.empty()) return "";
    std::ifstream in(CodePath(key), std::ios::binary);
    if (!in) return "";
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void SaveCode(const std::string& key, const std::string& code) {
    if (app.dataDir.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(app.dataDir + "segfault_code", ec);
    std::ofstream out(CodePath(key), std::ios::binary);
    out << code;
}

// ======================================================= console capture ===
static void AppendToCapture(const char* text) { app.consoleCapture += text; }
CaptureOutput::CaptureOutput() { app.consoleCapture.clear(); examOut = AppendToCapture; }
CaptureOutput::~CaptureOutput() { examOut = examOutToStdout; }

// ============================================================= sources =====
namespace sources {
static std::map<std::string, std::vector<std::string>> gSnippets;

void Load(const std::string& dir) {
    const char* files[] = {"exam1.cpp", "exam2_pointers.cpp", "linked_list.cpp"};
    for (const char* f : files) {
        std::ifstream in(dir + "/" + f);
        std::string line, name;
        std::vector<std::string> body;
        bool inside = false;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t b = line.find("// @begin ");
            if (b != std::string::npos) { name = line.substr(b + 10); body.clear(); inside = true; continue; }
            if (line.find("// @end") != std::string::npos && inside) { gSnippets[name] = body; inside = false; continue; }
            if (inside) body.push_back(line);
        }
    }
}

bool Has(const std::string& name) { return gSnippets.count(name) > 0; }

const std::vector<std::string>& Get(const std::string& name) {
    static const std::vector<std::string> missing = {"// source not found (resources/solutions)"};
    auto it = gSnippets.find(name);
    return it == gSnippets.end() ? missing : it->second;
}

std::string Text(const std::string& name) {
    std::string s;
    for (const auto& l : Get(name)) s += l + "\n";
    return s;
}

int FindLine(const std::string& name, const std::string& containing, int startAt) {
    const auto& lines = Get(name);
    for (int i = startAt; i < (int)lines.size(); i++)
        if (lines[i].find(containing) != std::string::npos) return i;
    return -1;
}
} // namespace sources
