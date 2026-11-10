#include "app.h"
#include "auth.h"
#include "streak.h"
#include "runner.h"
#include "theme.h"
#include "ui.h"
#include "content/content.h"
#include "exam_output.h"
#include <cctype>
#include <ctime>
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
    return (s >= 0 && s < S_COUNT) ? ui::Tone(c[s]) : ui::col::accent;
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
    {   // today counts as a day of practice
        std::time_t now = std::time(nullptr);
        std::tm tm = *std::localtime(&now);
        days.insert(streak::Format(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday));
    }
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
        else if (key == "theme") in >> theme;
        else if (key == "avatar") in >> avatarColour;
        else if (key == "trail") in >> trail;
        else if (key == "cursor") in >> customCursor;
        else if (key == "day") { std::string v; in >> v; days.insert(v); }
        else { std::string rest; std::getline(in, rest); }
    }
}

void Progress::Save() const {
    if (path.empty()) return;
    std::ofstream out(path);
    if (!out) return;
    out << "xp " << xp << "\nanswered " << quizAnswered << "\ncorrect " << quizCorrect << "\n";
    if (!theme.empty()) out << "theme " << theme << "\n";
    if (avatarColour) out << "avatar " << avatarColour << "\n";
    if (!trail) out << "trail 0\n";
    if (!customCursor) out << "cursor 0\n";
    for (auto& d : done) out << "done " << d << "\n";
    for (int m : mistakes) out << "mistake " << m << "\n";
    for (auto& b : best) out << "best " << b.first << " " << b.second << "\n";
    for (auto& d : days) out << "day " << d << "\n";
}

void Progress::Reset() {
    std::string p = path, t = theme;
    int a = avatarColour;
    bool tr = trail;
    bool cur = customCursor;
    *this = Progress();
    path = p;
    theme = t;
    avatarColour = a;
    trail = tr;
    customCursor = cur;
    Save();
}

float SectionMastery(int section) {
    int total = 0, got = 0;
    for (const Lesson& l : Lessons()) if (l.section == section) { total += 2; got += app.progress.Has("lesson:" + l.id) ? 2 : 0; }
    for (const Problem& p : Problems()) if (p.section == section) { total += 3; got += app.progress.Has("solved:" + p.id) ? 3 : 0; }
    for (const Question& q : Questions()) if (q.section == section) { total++; got += app.progress.Has("q:" + std::to_string(q.id)); }
    return total ? (float)got / total : 0;
}

// ============================================================== accounts ===
static std::string ToLowerCopy(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

void SignIn(const std::string& username) {
    bool prevTrail = app.progress.trail;
    bool prevCursor = app.progress.customCursor;
    app.user = username;
    app.lastUser = username;
    app.userDir = auth::UserStore(app.dataDir + "segfault_users").UserDir(username);
    app.progress = Progress();
    app.progress.trail = prevTrail;
    app.progress.customCursor = prevCursor;
    app.progress.Load(app.userDir + "segfault_progress.txt");
    if (!app.progress.theme.empty()) ui::ApplyTheme(ui::ThemeIndex(app.progress.theme));
    else app.progress.theme = ui::Themes()[ui::CurrentTheme()].id;
    app.progress.Save();
    SaveAppSettings();
    app.Go(SceneId::Home);
}

std::vector<LeaderRow> LoadLeaderboard() {
    std::vector<LeaderRow> rows;
    auth::UserStore store(app.dataDir + "segfault_users");
    for (const std::string& name : store.Usernames()) {
        LeaderRow r;
        r.name = name;
        r.you = !app.user.empty() && ToLowerCopy(name) == ToLowerCopy(app.user);
        Progress p;
        if (r.you) p = app.progress;   // the newest numbers, even before they are saved
        else p.Load(store.UserDir(name) + "segfault_progress.txt");
        r.xp = p.xp;
        r.level = p.Level();
        r.answered = p.quizAnswered;
        r.correct = p.quizCorrect;
        r.avatarColour = p.avatarColour;
        // only problems and lessons that exist, the same as the home screen counts
        for (const Problem& pr : Problems()) r.solved += p.Has("solved:" + pr.id);
        for (const Lesson& l : Lessons()) r.lessons += p.Has("lesson:" + l.id);
        for (const auto& b : p.best)
            if (b.first.rfind("mock:", 0) == 0) r.bestMock = std::max(r.bestMock, b.second);
        rows.push_back(r);
    }
    return rows;
}

static std::string SettingsPath() { return app.dataDir + "segfault_settings.txt"; }
static std::string LastThemePath() { return app.dataDir + "segfault_theme.txt"; }

void SaveAppSettings() {
    const char* themeId = ui::Themes()[ui::CurrentTheme()].id;
    std::ofstream(LastThemePath()) << themeId << "\n";
    std::ofstream out(SettingsPath());
    if (!out) return;
    out << "theme " << themeId << "\n";
    out << "trail " << (app.progress.trail ? 1 : 0) << "\n";
    out << "cursor " << (app.progress.customCursor ? 1 : 0) << "\n";
    if (!app.user.empty()) out << "user " << app.user << "\n";
    if (!app.lastUser.empty()) out << "last_user " << app.lastUser << "\n";
    if (!runner::PreferredLabel().empty()) out << "compiler " << runner::PreferredLabel() << "\n";
}

void SelectTheme(int index) {
    ui::ApplyTheme(index);
    const char* id = ui::Themes()[ui::CurrentTheme()].id;
    if (!app.user.empty()) { app.progress.theme = id; app.progress.Save(); }
    SaveAppSettings();
}

void LoadLastTheme(bool restoreSession) {
    std::ifstream legacy(LastThemePath());
    std::string id;
    if (legacy >> id) ui::ApplyTheme(ui::ThemeIndex(id));

    std::ifstream in(SettingsPath());
    std::string key, savedUser;
    while (in >> key) {
        if (key == "theme") { std::string t; in >> t; ui::ApplyTheme(ui::ThemeIndex(t)); }
        else if (key == "trail") in >> app.progress.trail;
        else if (key == "cursor") in >> app.progress.customCursor;
        else if (key == "user") in >> savedUser;
        else if (key == "last_user") in >> app.lastUser;
        else if (key == "compiler") {
            std::string comp;
            std::getline(in >> std::ws, comp);
            if (!comp.empty() && comp.back() == '\r') comp.pop_back();
            runner::SetPreferredLabel(comp);
        } else {
            std::string rest;
            std::getline(in, rest);
        }
    }
    if (restoreSession && !savedUser.empty()) {
        auth::UserStore store(app.dataDir + "segfault_users");
        for (const std::string& name : store.Usernames()) {
            if (ToLowerCopy(name) == ToLowerCopy(savedUser)) {
                SignIn(name);
                break;
            }
        }
    }
}

void SignOut() {
    bool keepTrail = app.progress.trail;
    bool keepCursor = app.progress.customCursor;
    app.user.clear();
    app.userDir.clear();
    app.progress = Progress();   // no path: nothing can be saved until the next login
    app.progress.trail = keepTrail;
    app.progress.customCursor = keepCursor;
    SaveAppSettings();
    app.Go(SceneId::Login);
}

// ========================================================= saved code ======
static std::string CodePath(const std::string& key) {
    std::string safe;
    for (char c : key) safe += (std::isalnum((unsigned char)c) || c == '_' || c == '-') ? c : '_';
    return app.userDir + "segfault_code/" + safe + ".cpp";
}

std::string LoadSavedCode(const std::string& key) {
    if (app.userDir.empty()) return "";
    std::ifstream in(CodePath(key), std::ios::binary);
    if (!in) return "";
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void SaveCode(const std::string& key, const std::string& code) {
    if (app.userDir.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(app.userDir + "segfault_code", ec);
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
