// ============================================================================
// editor_app.cpp  -  SegFault Editor: your own C and C++ files, outside the
// exercises. Make files, write anything, run it with some input and see what
// it prints. Files are kept per student (<userDir>/scratch/, or next to the
// app when nobody is logged in) and saved as you type.
// ============================================================================
#include "app/app.h"
#include "app/editor.h"
#include "app/runner.h"
#include "app/theme.h"
#include "app/ui.h"
#include "scenes/workspace.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace ui;
namespace fs = std::filesystem;

namespace {

const char* kCppStarter =
    "#include <iostream>\n"
    "using namespace std;\n"
    "\n"
    "int main() {\n"
    "    \n"
    "    return 0;\n"
    "}\n";
const char* kCStarter =
    "#include <stdio.h>\n"
    "\n"
    "int main(void) {\n"
    "    \n"
    "    return 0;\n"
    "}\n";
const char* kHello =
    "#include <iostream>\n"
    "#include <string>\n"
    "using namespace std;\n"
    "\n"
    "// Your own space: write anything, press Run (Ctrl+Enter).\n"
    "// What you type in Input is what cin reads.\n"
    "int main() {\n"
    "    string name;\n"
    "    cout << \"What's your name? \";\n"
    "    cin >> name;\n"
    "    cout << \"Hello, \" << name << \"!\" << endl;\n"
    "    return 0;\n"
    "}\n";

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct OutLine { std::string text; Color colour; };

class EditorScene : public Scene {
public:
    ~EditorScene() override { SaveNow(); }
    const char* Title() const override { return "SegFault Editor"; }
    ai::Screen AiInfo() const override {
        ai::Screen s;
        s.isEditor = true;
        s.label = "SegFault Editor" + (current.empty() ? std::string() : " - " + current);
        if (current.empty()) return s;
        std::string c = "SegFault Editor (the student's own code, not an exam problem). File: " + current + "\n\n";
        c += ai::NumberLines(code.Text());
        std::string in = stdinBox.Text();
        if (!in.empty()) c += "\nInput given to the program:\n" + in.substr(0, 1000) + "\n";
        if (hasResult) {
            s.status = last.status;
            c += std::string("\nLast run: ") + (last.status == RunReport::Accepted ? "finished normally (exit code 0)" : StatusText(last.status)) + "\n";
            if (last.status == RunReport::RuntimeError) c += "Exit: " + ExitCodeText(last.exitCode) + "\n";
            if (!last.compileLog.empty()) c += "Compiler output:\n" + last.compileLog.substr(0, 3000) + "\n";
            if (!last.stdoutText.empty()) c += "It printed:\n" + last.stdoutText.substr(0, 1500) + "\n";
        }
        s.context = c;
        return s;
    }

    void Enter(const std::string&) override {
        t = 0;
        std::string d = Dir();
        if (d != dir) {   // someone else logged in: start again
            dir = d;
            current.clear();
        }
        Refresh();
        if (files.empty()) {   // the first visit: a file to start from
            std::error_code ec;
            fs::create_directories(dir, ec);
            std::ofstream(dir + "hello.cpp", std::ios::binary) << kHello;
            std::ofstream(dir + "hello.cpp.in", std::ios::binary) << "Ada\n";
            Refresh();
        }
        if (current.empty() || std::find(files.begin(), files.end(), current) == files.end())
            Open(files.empty() ? "" : files[0]);
        code.Focus(8001);
    }

    void Frame(float dt) override {
        t += dt;
        const bool input = InputEnabled();
        if (code.TakeChanged()) { dirtySince = t; code.SetErrorLines({}); }
        if (stdinBox.TakeChanged()) dirtySince = t;
        if (dirtySince >= 0 && t - dirtySince > 0.8f) SaveNow();
        if (job.Ready()) ShowResult(job.Take());

        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (input && !current.empty() && !job.Busy() &&
            (IsKeyPressed(KEY_F5) || (ctrl && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))))
            Run();
        if (input && ctrl && IsKeyPressed(KEY_S)) { SaveNow(); Toast("Saved " + current, col::success); }

        Sidebar({24, 84, 236, 692}, input);
        Toolbar({276, 84, 980, 52}, input);
        Rectangle ed = {276, 144, 980, 396};
        if (current.empty()) {
            Panel(ed);
            TextCenter("Make a file to start", {ed.x, ed.y + ed.height / 2 - 10, ed.width, 20}, 15, col::dim, F_MEDIUM);
        } else {
            code.fontSize = 15.5f;
            code.Frame(ed, 8001);
        }
        InputPanel({276, 552, 316, 224});
        OutputPanel({604, 552, 652, 224});
    }

private:
    std::string dir, current;
    std::vector<std::string> files;
    CodeEditor code, stdinBox;
    float t = 0, dirtySince = -1;
    // new file / delete
    bool creating = false;
    std::string newName, newError;
    bool newIsC = false;
    std::string confirmDelete;
    Scroll listScroll, outScroll;
    // running
    FreeRunJob job;
    bool hasResult = false;
    RunReport last;
    std::vector<OutLine> out;

    static std::string Dir() {
        return app.userDir.empty() ? app.dataDir + "segfault_scratch/" : app.userDir + "scratch/";
    }

    void Refresh() {
        files.clear();
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            std::string ext = it->path().extension().string();
            if (ext == ".cpp" || ext == ".c") files.push_back(it->path().filename().string());
        }
        std::sort(files.begin(), files.end());
    }

    void SaveNow() {
        if (current.empty() || dirtySince < 0) return;
        std::error_code ec;
        fs::create_directories(dir, ec);
        std::ofstream(dir + current, std::ios::binary) << code.Text();
        std::string in = stdinBox.Text();
        if (!in.empty()) std::ofstream(dir + current + ".in", std::ios::binary) << in;
        else fs::remove(dir + current + ".in", ec);
        dirtySince = -1;
    }

    void Open(const std::string& name) {
        SaveNow();
        current = name;
        code.SetText(name.empty() ? "" : ReadFile(dir + name));
        stdinBox.SetText(name.empty() ? "" : ReadFile(dir + name + ".in"));
        code.SetErrorLines({});
        hasResult = false;
        out.clear();
        outScroll.Reset();
        dirtySince = -1;
    }

    void Create() {
        std::string n = newName;
        while (!n.empty() && n.back() == ' ') n.pop_back();
        // "list.c" typed in full picks the language too
        if (n.size() > 4 && n.compare(n.size() - 4, 4, ".cpp") == 0) { n.resize(n.size() - 4); newIsC = false; }
        else if (n.size() > 2 && n.compare(n.size() - 2, 2, ".c") == 0) { n.resize(n.size() - 2); newIsC = true; }
        if (n.empty()) { newError = "Give it a name"; return; }
        for (char ch : n)
            if (!(std::isalnum((unsigned char)ch) || ch == '_' || ch == '-')) { newError = "Letters, digits, - and _ only"; return; }
        std::string file = n + (newIsC ? ".c" : ".cpp");
        if (std::find(files.begin(), files.end(), file) != files.end()) { newError = file + " already exists"; return; }
        std::error_code ec;
        fs::create_directories(dir, ec);
        std::ofstream f(dir + file, std::ios::binary);
        if (!f) { newError = "Couldn't create the file"; return; }
        f << (newIsC ? kCStarter : kCppStarter);
        f.close();
        creating = false;
        Refresh();
        Open(file);
        code.Focus(8001);
    }

    void Delete(const std::string& name) {
        std::error_code ec;
        fs::remove(dir + name, ec);
        fs::remove(dir + name + ".in", ec);
        if (name == current) { dirtySince = -1; current.clear(); }
        Refresh();
        if (current.empty()) Open(files.empty() ? "" : files[0]);
        Toast("Deleted " + name, col::dim);
    }

    void Run() {
        SaveNow();
        hasResult = false;
        out.clear();
        code.SetErrorLines({});
        job.Start(current, code.Text(), stdinBox.Text());
    }

    void Add(const std::string& text, Color c) {
        for (const std::string& l : SplitLines(text)) out.push_back({l, c});
    }

    void ShowResult(const RunReport& r) {
        last = r;
        hasResult = true;
        out.clear();
        outScroll.Reset();
        switch (r.status) {
            case RunReport::CompileError:
                Add(r.compileLog, col::text);
                code.SetErrorLines(r.errorLines);
                break;
            case RunReport::NoCompiler:
                Add("No C / C++ compiler was found on this computer.\nInstall g++ (MinGW) or Visual Studio, then restart the app.", col::text);
                break;
            case RunReport::NeedsCpp:
                Add(r.compileLog, col::text);
                break;
            default:
                if (r.stdoutText.empty()) out.push_back({"(nothing printed)", col::faint});
                else Add(r.stdoutText, col::synText);
                if (r.status == RunReport::RuntimeError) {
                    out.push_back({"", col::text});
                    out.push_back({"-> " + ExitCodeText(r.exitCode), col::danger});
                } else if (r.status == RunReport::TimeLimit) {
                    out.push_back({"", col::text});
                    out.push_back({"-> Stopped after 5 s: an endless loop, or waiting for input that isn't there?", col::warn});
                }
                if (!r.compileLog.empty()) {   // warnings
                    out.push_back({"", col::text});
                    out.push_back({"Warnings:", col::warn});
                    Add(r.compileLog, Mix(col::warn, col::text, 0.4f));
                }
                break;
        }
    }

    // ------------------------------------------------------------------ UI --
    void Sidebar(Rectangle r, bool input) {
        Panel(r);
        Text("Files", r.x + 18, r.y + 16, 17, col::heading, F_HEAVY);
        TextRight(std::to_string(files.size()), r.x + r.width - 18, r.y + 19, 13, col::faint, F_BOLD);
        float y = r.y + 50;
        if (!creating) {
            if (Button({r.x + 14, y, r.width - 28, 36}, "+  New file", BTN_DEFAULT, input, 14)) {
                creating = true; newName.clear(); newError.clear(); confirmDelete.clear();
                FocusWidget(8003);
            }
            y += 48;
        } else {
            Rectangle box = {r.x + 14, y, r.width - 28, 36};
            bool submit = TextBox(box, newName, 8003, 28, false, "name, e.g. lists");
            y += 44;
            for (int i = 0; i < 2; i++) {   // .cpp / .c
                Rectangle b = {r.x + 14 + i * 58.0f, y, 52, 28};
                bool on = newIsC == (i == 1);
                bool hover = input && MouseIn(b);
                RoundRect(b, 8, on ? col::accent : hover ? col::panelHi : Alpha(col::panelHi, 0.6f));
                TextCenter(i ? ".c" : ".cpp", b, 13, on ? OnColor(col::accent) : col::text, F_MONOBOLD);
                if (hover) WantHand();
                if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) newIsC = i == 1;
            }
            Rectangle ok = {r.x + r.width - 14 - 64, y - 2, 64, 32};
            if (Button(ok, "Create", BTN_PRIMARY, input, 13) || submit) Create();
            y += 34;
            if (!newError.empty()) { Text(newError, r.x + 16, y, 12.5f, col::danger, F_MEDIUM); y += 18; }
            if (Button({r.x + 14, y + 2, r.width - 28, 28}, "Cancel", BTN_GHOST, input, 12.5f) || (input && IsKeyPressed(KEY_ESCAPE)))
                creating = false;
            y += 40;
        }
        DrawRectangleRec({r.x + 12, y, r.width - 24, 1}, col::border);
        y += 8;

        Rectangle view = {r.x + 6, y, r.width - 12, r.y + r.height - 8 - y};
        const float rh = 42;
        listScroll.Update(view, files.size() * rh + 4);
        BeginClip(view);
        std::string toOpen, toDelete;
        for (size_t i = 0; i < files.size(); i++) {
            const std::string& f = files[i];
            Rectangle row = {view.x + 4, view.y + i * rh - listScroll.y, view.width - 14, rh - 4};
            if (row.y + row.height < view.y || row.y > view.y + view.height) continue;
            bool hover = input && MouseIn(row) && MouseIn(view);
            bool sel = f == current;
            if (sel) { RoundRect(row, 9, Alpha(col::accent, 0.16f)); DrawRectangleRec({row.x, row.y + 9, 3, row.height - 18}, col::accent); }
            else if (hover) RoundRect(row, 9, col::panelHi);
            bool isC = f.size() > 2 && f.compare(f.size() - 2, 2, ".c") == 0;
            std::string base = f.substr(0, f.rfind('.'));
            if (confirmDelete == f) {
                Text("Delete?", row.x + 12, row.y + 11, 13.5f, col::danger, F_BOLD);
                if (Button({row.x + row.width - 104, row.y + 5, 48, 28}, "Yes", BTN_DANGER, input, 12.5f)) toDelete = f;
                if (Button({row.x + row.width - 52, row.y + 5, 48, 28}, "No", BTN_GHOST, input, 12.5f)) confirmDelete.clear();
                continue;
            }
            Color tag = isC ? col::accent2 : col::accent;
            Rectangle tr = {row.x + 10, row.y + 9, 34, 20};
            RoundRect(tr, 5, Alpha(tag, 0.16f));
            TextCenter(isC ? "C" : "C++", tr, 11, tag, F_MONOBOLD);
            Text(Ellipsize(base, 14.5f, F_BOLD, row.width - 92), row.x + 54, row.y + 10, 14.5f, sel ? col::heading : col::text, F_BOLD);
            Rectangle bin = {row.x + row.width - 30, row.y + 7, 24, 24};
            if (hover) {   // a small bin
                bool bh = MouseIn(bin);
                Color bc = bh ? col::danger : col::faint;
                RoundRect({bin.x + 7, bin.y + 8, 10, 11}, 2, Alpha(bc, 0.0f));
                RoundRectLines({bin.x + 7, bin.y + 8, 10, 11}, 2, 1.5f, bc);
                DrawRectangleRec({bin.x + 5, bin.y + 6, 14, 1.5f}, bc);
                DrawRectangleRec({bin.x + 10, bin.y + 4, 4, 2}, bc);
                WantHand();
                if (bh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { confirmDelete = f; continue; }
            }
            if (hover && !MouseIn(bin) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && f != current) toOpen = f;
        }
        EndClip();
        ScrollBar(view, files.size() * rh + 4, listScroll);
        if (files.empty() && !creating)
            TextCenter("No files yet", {view.x, view.y + 20, view.width, 20}, 13.5f, col::faint, F_MEDIUM);
        if (!toDelete.empty()) { confirmDelete.clear(); Delete(toDelete); }
        if (!toOpen.empty()) { confirmDelete.clear(); Open(toOpen); }
    }

    void Toolbar(Rectangle r, bool input) {
        Panel(r);
        float x = r.x + 18;
        if (!current.empty()) {
            Text(current, x, r.y + 15, 17, col::heading, F_MONOBOLD);
            x += Measure(current, 17, F_MONOBOLD).x + 14;
            bool saved = dirtySince < 0;
            DrawCircleV({x + 4, r.y + 26}, 4, saved ? col::success : col::warn);
            Text(saved ? "saved" : "editing", x + 14, r.y + 18, 13, col::faint, F_MEDIUM);
        }
        Rectangle run = {r.x + r.width - 126, r.y + 8, 114, 36};
        bool busy = job.Busy();
        if (Button(run, busy ? "Running" : "     Run", BTN_SUCCESS, input && !busy && !current.empty(), 15)) Run();
        if (!busy) {   // a play triangle
            Color on = OnColor(col::success);
            float cx = run.x + 34, cy = run.y + 18;
            DrawTriangle({cx - 5, cy - 7}, {cx - 5, cy + 7}, {cx + 7, cy}, on);
        }
        TextRight("Ctrl+Enter", run.x - 14, r.y + 19, 12.5f, col::faint, F_MEDIUM);
        CompilerChip({run.x - 100 - 290, r.y + 9, 280, 34});
    }

    void InputPanel(Rectangle r) {
        Panel(r);
        Text("Input", r.x + 16, r.y + 13, 15, col::heading, F_BOLD);
        Text("what cin / scanf read", r.x + 66, r.y + 15, 12.5f, col::faint, F_MEDIUM);
        stdinBox.completion = false;
        stdinBox.plain = true;
        stdinBox.fontSize = 14.5f;
        stdinBox.Frame({r.x + 8, r.y + 42, r.width - 16, r.height - 50}, 8002);
    }

    void OutputPanel(Rectangle r) {
        Panel(r);
        Text("Output", r.x + 16, r.y + 13, 15, col::heading, F_BOLD);
        float bx = r.x + 84;
        if (job.Busy()) {
            static const char* dots[] = {"", ".", "..", "..."};
            Badge(bx, r.y + 11, std::string("Compiling and running") + dots[(int)(t * 4) % 4], col::warn, 12);
        } else if (hasResult) {
            std::string label;
            Color c = col::success;
            switch (last.status) {
                case RunReport::Accepted: label = "Finished"; break;
                case RunReport::CompileError: label = "Compile error"; c = col::danger; break;
                case RunReport::RuntimeError: {
                    unsigned u = (unsigned)last.exitCode;
                    label = u >= 0xC0000000u || u == 3 ? ExitCodeText(last.exitCode) : "Exit code " + std::to_string(last.exitCode);
                    c = u >= 0xC0000000u || u == 3 ? col::danger : col::warn;
                    break;
                }
                case RunReport::TimeLimit: label = "Time limit"; c = col::warn; break;
                default: label = "Can't run"; c = col::danger; break;
            }
            float w = Badge(bx, r.y + 11, label, c, 12);
            char ms[48];
            if (last.status == RunReport::Accepted || last.status == RunReport::RuntimeError) {
                std::snprintf(ms, sizeof(ms), "ran in %.0f ms", last.runMs);
                Text(ms, bx + w + 10, r.y + 15, 12.5f, col::faint, F_MEDIUM);
            }
        }
        Rectangle view = {r.x + 8, r.y + 42, r.width - 16, r.height - 50};
        RoundRect(view, 8, col::codeBg);
        const float fs = 14.5f, lh = CodeLineHeight(fs);
        float contentH = out.size() * lh + 16;
        outScroll.Update(view, contentH);
        BeginClip(view);
        if (out.empty() && !job.Busy())
            Text("Press Run to see what your program prints", view.x + 14, view.y + 12, 13.5f, col::faint, F_MEDIUM);
        for (size_t i = 0; i < out.size(); i++) {
            float y = view.y + 8 + i * lh - outScroll.y;
            if (y + lh < view.y || y > view.y + view.height) continue;
            Text(out[i].text, view.x + 12, y + (lh - fs) / 2, fs, out[i].colour, F_MONO);
        }
        EndClip();
        ScrollBar(view, contentH, outScroll);
    }
};

} // namespace

Scene* CreateEditorScene() { return new EditorScene(); }
