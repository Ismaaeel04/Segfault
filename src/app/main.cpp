// ============================================================================
// Segfault  -  an interactive C / C++ learning app built with raylib.
//   Study     : lessons by section, with step-by-step code walkthroughs
//   Practice  : LeetCode-style problems - write code, run the tests
//   Mock Exam : a timed coding exam, graded by hidden tests
//   Quiz      : quick multiple-choice practice
//
// main.cpp: window, DPI-aware virtual canvas, top bar, scene loop, self test.
// ============================================================================
#include "raylib.h"
#include "rlgl.h"
#include "app.h"
#include "runner.h"
#include "theme.h"
#include "ui.h"
#include "content/content.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

using namespace ui;

static std::unique_ptr<Scene> gScenes[(int)SceneId::Count];

static void CreateScenes() {
    gScenes[(int)SceneId::Home].reset(CreateHomeScene());
    gScenes[(int)SceneId::Study].reset(CreateStudyScene());
    gScenes[(int)SceneId::Practice].reset(CreatePracticeScene());
    gScenes[(int)SceneId::Mock].reset(CreateMockScene());
    gScenes[(int)SceneId::Quiz].reset(CreateQuizScene());
    gScenes[(int)SceneId::Visualizer].reset(CreateVisualizerScene());
    gScenes[(int)SceneId::ListLab].reset(CreateListLabScene());
    gScenes[(int)SceneId::Grid].reset(CreateGridScene());
    gScenes[(int)SceneId::Login].reset(CreateLoginScene());
}

static std::string FindResourceDir() {
    std::string base = GetApplicationDirectory();
    const std::string candidates[] = {base + "resources", base + "../resources", base + "../../resources",
                                      "resources", "../resources", "../../resources"};
    for (const auto& c : candidates)
        if (FileExists((c + "/fonts/Inter-Regular.ttf").c_str())) return c;
    return "resources";
}

static std::string FindSolutionsDir(const std::string& res) {
    const std::string candidates[] = {res + "/solutions", res + "/../src/solutions",
                                      "src/solutions", "../src/solutions", "../../src/solutions"};
    for (const auto& c : candidates)
        if (FileExists((c + "/exam1.cpp").c_str())) return c;
    return res + "/solutions";
}

// ---------------------------------------------------------------- top bar ---
static void ThemeButton(Rectangle r) {
    if (Button(r, "       Theme", BTN_GHOST, true, 13.5f)) app.themePickerOpen = true;
    const Color dots[3] = {col::accent, col::accent2, col::pointer};
    for (int i = 0; i < 3; i++) DrawCircleV({r.x + 13 + i * 6.5f, r.y + r.height / 2}, 4, dots[i]);
}

static bool TopBar(Scene* scene) {
    DrawRectangleRec({0, 0, W, 64}, col::bg2);
    DrawRectangleGradientEx({0, 62.5f, W, 1.5f}, col::accent, col::accent, col::accent2, col::accent2);
    bool back = false;
    if (app.current == SceneId::Home || app.current == SceneId::Login) {
        GradientRoundRect({24, 15, 34, 34}, 9, col::accent, Mix(col::accent, col::accent2, 0.7f));
        RoundRect({33, 24, 16, 16}, 3, col::bg2);
        DrawCircleV({41, 32}, 3.5f, col::pointer);
        Text("Segfault", 70, 13, 22, col::heading, F_DISPLAY);
        Text("Learn C & C++ by writing it", 71, 40, 13, col::dim, F_MEDIUM);
    } else {
        back = Button({20, 13, 104, 38}, app.hasReturn ? "<  Back" : "<  Home", BTN_GHOST, true, 15);
        Text(scene->Title(), 142, 19, 21, col::heading, F_HEAVY);
    }
    if (app.current == SceneId::Login) { ThemeButton({W - 124, 15, 100, 34}); return false; }
    const Progress& p = app.progress;
    float bx = W - 252;
    ThemeButton({bx - 112, 15, 100, 34});
    // Only on Home, so nobody logs out by accident in the middle of an exam.
    if (app.current == SceneId::Home && !app.user.empty()) {
        if (Button({bx - 208, 15, 88, 34}, "Log out", BTN_GHOST, true, 13.5f)) app.signOutRequested = true;
        TextRight(app.user, bx - 220, 23, 14.5f, col::text, F_BOLD);
    }
    RoundRect({bx, 15, 228, 34}, 17, col::panel);
    RoundRectLines({bx, 15, 228, 34}, 17, 1, Alpha(col::border, 0.7f));
    Badge(bx + 5, 20, "LV " + std::to_string(p.Level()), col::warn, 12.5f);
    ProgressBar({bx + 70, 29, 92, 6}, p.LevelProgress(), col::accent2);
    TextRight(std::to_string(p.xp) + " XP", bx + 216, 23, 14, col::text, F_BOLD);
    return back;
}

// -------------------------------------------------------------- self test ---
// --selftest: compiles every problem's reference solution with every compiler
// found and checks that all its tests pass; checks lesson links and content.
static int SelfTest(bool quick) {
    int fails = 0;
    auto fail = [&](const std::string& m) { printf("FAIL: %s\n", m.c_str()); fflush(stdout); fails++; };
    std::string res = FindResourceDir();
    sources::Load(FindSolutionsDir(res));
    runner::DetectNow(res);
    printf("compilers:");
    for (const Compiler& c : runner::Compilers()) printf(" [%s]", c.label.c_str());
    printf("\n");
    if (runner::Compilers().empty()) fail("no compiler found");
    for (const Compiler& c : runner::Compilers()) {
        int checked = 0;
        for (const Problem& p : Problems()) {
            if (p.cppOnly && !c.cpp) continue;
            RunReport r = RunCode(p, p.solution, &c);
            if (r.status != RunReport::Accepted) {
                fail("[" + c.label + "] " + p.id + ": " + StatusText(r.status) + " " + std::to_string(r.passed) + "/" + std::to_string(r.total));
                if (!r.compileLog.empty()) printf("%s\n", r.compileLog.substr(0, 1500).c_str());
                for (auto& cs : r.cases) if (!cs.ok) printf("   case %d: in=%s exp=[%s] got=[%s]\n", cs.id, cs.input.c_str(), cs.expected.c_str(), cs.got.c_str());
            }
            if (r.total < p.exampleCases) fail(p.id + ": fewer cases than examples");
            if (!quick) {
                RunReport s = RunCode(p, p.starter, &c);
                if (s.status == RunReport::CompileError) { fail("[" + c.label + "] starter does not compile: " + p.id); printf("%s\n", s.compileLog.substr(0, 800).c_str()); }
                if (s.status == RunReport::Accepted) fail("[" + c.label + "] starter already passes: " + p.id);
            }
            checked++;
        }
        printf("  %s: %d problems checked\n", c.label.c_str(), checked);
        if (quick) break;
    }
    for (const Lesson& l : Lessons()) for (const Block& b : l.blocks) {
        if (b.type == BlockType::Practice) {
            size_t start = 0;
            while (start <= b.text.size()) {
                size_t e = b.text.find(',', start);
                std::string id = b.text.substr(start, e == std::string::npos ? std::string::npos : e - start);
                if (!ProblemById(id)) fail("lesson " + l.id + " links missing problem " + id);
                if (e == std::string::npos) break;
                start = e + 1;
            }
        }
        if (b.type == BlockType::Demo && b.scene == SceneId::Visualizer) {
            bool ok = false;
            for (const MemProgram& m : MemPrograms()) ok |= m.id == b.param;
            if (!ok) fail("lesson " + l.id + " links missing program " + b.param);
        }
        if (b.type == BlockType::Walk) {
            int n = 1;
            for (char ch : b.text) n += ch == '\n';
            for (const WalkStep& s : b.steps) if (s.line < 0 || s.line >= n) fail("lesson " + l.id + ": walkthrough line out of range");
        }
        if (b.type == BlockType::Check && b.options.size() < 2) fail("lesson " + l.id + ": check needs options");
    }
    for (const MemProgram& m : MemPrograms()) {
        MemState st;
        for (const MemStep& s : m.steps) {
            if (s.line < 0 || s.line >= (int)m.code.size()) fail("program " + m.id + ": bad line");
            s.apply(st);
        }
    }
    // Every theme has to stay readable.
    auto need = [&](const Theme& t, const char* what, Color fg, Color bg, float min) {
        float c = ContrastRatio(fg, bg);
        if (c < min) fail(std::string("theme ") + t.id + ": " + what + " contrast " + std::to_string(c) + " < " + std::to_string(min));
    };
    for (int i = 0; i < (int)Themes().size(); i++) {
        const Theme& t = Themes()[i];
        ApplyTheme(i);
        for (Color surface : {t.bg, t.bg2, t.panel, t.panelHi}) {
            need(t, "heading", t.heading, surface, 4.5f);
            need(t, "text", t.text, surface, 4.5f);
            need(t, "dim", t.dim, surface, 4.5f);
            need(t, "faint", t.faint, surface, 2.5f);
            for (Color c : {t.warn, t.danger, t.success, t.pointer, t.heap, t.stack, t.accent2})
                need(t, "status colour", c, surface, 3.0f);
        }
        for (Color c : {t.synText, t.synType, t.synKey, t.synNum, t.synStr, t.synFn, t.synPre, t.synPunct})
            need(t, "syntax", c, t.codeBg, 4.5f);
        need(t, "comment", t.synCom, t.codeBg, 3.0f);
        need(t, "primary button", OnColor(t.accent), t.accent, 3.0f);
        for (int s = 0; s < S_COUNT; s++) need(t, "section colour", SectionColor(s), t.panel, 3.0f);
        for (int d = 0; d < 3; d++) need(t, "difficulty colour", DifficultyColor(d), t.panel, 3.0f);
    }
    ApplyTheme(0);
    printf("themes: %zu checked\n", Themes().size());

    printf("selftest: %zu problems, %zu lessons, %zu questions, %zu programs - %d failure(s)\n",
           Problems().size(), Lessons().size(), Questions().size(), MemPrograms().size(), fails);
    return fails ? 1 : 0;
}

int main(int argc, char** argv) {
    ApplyTheme(0);
    if (argc >= 2 && std::strcmp(argv[1], "--selftest") == 0) return SelfTest(argc >= 3 && std::strcmp(argv[2], "quick") == 0);

    // Optional: --shot <scene> <param> <file.png> [frames] [theme] renders one screen in
    // a hidden window (it never takes the focus) and exits.
    const char* shotFile = nullptr;
    int shotFrame = 50;
    int shotScene = -1;
    std::string shotParam;
    if (argc >= 5 && std::strcmp(argv[1], "--shot") == 0) {
        shotScene = std::atoi(argv[2]);
        shotParam = argv[3];
        if (shotParam == "-") shotParam.clear();
        shotFile = argv[4];
        if (argc >= 6) shotFrame = std::atoi(argv[5]);
    }

    // SEGFAULT_BENCH=<width>x<height> with --shot: draws as fast as it can at
    // that window size and prints the average time per frame.
    const char* bench = shotFile ? std::getenv("SEGFAULT_BENCH") : nullptr;
    unsigned flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | (bench ? 0 : FLAG_VSYNC_HINT);
#ifdef __APPLE__
    flags |= FLAG_WINDOW_HIGHDPI;     // sharp text on Retina screens
#endif
    if (shotFile) flags |= FLAG_WINDOW_HIDDEN;
    SetConfigFlags(flags);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow((int)W, (int)H, "Segfault - Learn C & C++");
    SetExitKey(KEY_NULL);
    SetTargetFPS(bench ? 0 : 60);
    SetWindowMinSize(800, 500);
    double benchStart = 0, benchLast = 0, benchWorst = 0;
    int benchSlow = 0;
    const bool benchTour = bench && std::getenv("SEGFAULT_BENCH_TOUR");
    if (bench) {
        int bw = 0, bh = 0;
        if (std::sscanf(bench, "%dx%d", &bw, &bh) == 2 && bw > 0 && bh > 0) SetWindowSize(bw, bh);
    }
    if (!shotFile) {
        // Size the window for the screen's DPI so text is big and sharp
        // (on a 150% Windows laptop the canvas is drawn at 1.5x, not stretched).
        int mon = GetCurrentMonitor();
        float mw = (float)GetMonitorWidth(mon), mh = (float)GetMonitorHeight(mon);
        Vector2 dpi = GetWindowScaleDPI();
        float d = std::fmax(1.0f, dpi.x);
#ifdef __APPLE__
        d = 1.0f;   // monitor size is already in points on macOS
#endif
        if (mw > 0 && mh > 0) {
            float s = std::fmin(d, std::fmin(mw * 0.92f / W, mh * 0.86f / H));
            if (mw < W * 1.05f || mh < H * 1.1f || d > 1.01f) {
                SetWindowSize((int)(W * s), (int)(H * s));
                SetWindowPosition((int)(mw - W * s) / 2, (int)(mh - H * s) / 2);
            }
        }
    }

    app.resourceDir = FindResourceDir();
    if (!InitFonts(app.resourceDir))
        TraceLog(LOG_WARNING, "Fonts not found in %s - using the default font", app.resourceDir.c_str());
    sources::Load(FindSolutionsDir(app.resourceDir));
    runner::StartDetection(app.resourceDir);
    app.dataDir = GetApplicationDirectory();
    LoadLastTheme();
    if (shotFile && argc >= 7) ApplyTheme(ThemeIndex(argv[6]));
    CreateScenes();

    if (shotFile) app.Go((SceneId)shotScene, shotParam);
    else app.Go(SceneId::Login);

    RenderTexture2D canvas = {};
    float currentScale = -1;
    float fade = 1;                       // scene switches fade in
    int frame = 0;

    while (!WindowShouldClose()) {
        // ---- fit the virtual canvas into the window (pixel exact) ----------
        float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
        float rw = (float)GetRenderWidth(), rh = (float)GetRenderHeight();
        float px = sw > 0 ? rw / sw : 1;                 // pixels per screen point (2 on Retina)
        float scale = std::fmin(rw / W, rh / H);         // pixels per canvas unit
        if (scale <= 0) scale = 1;
        if (std::fabs(scale - currentScale) > 0.001f) {
            if (canvas.id) UnloadRenderTexture(canvas);
            canvas = LoadRenderTexture((int)std::lround(W * scale), (int)std::lround(H * scale));
            SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
            ui::SetScale(scale);
            ClearFontCache();
            currentScale = scale;
        }
        float dw = canvas.texture.width / px, dh = canvas.texture.height / px;   // in screen points
        float offX = std::floor((sw - dw) * 0.5f), offY = std::floor((sh - dh) * 0.5f);
        SetMouseOffset((int)-offX, (int)-offY);
        SetMouseScale(px / scale, px / scale);

        // ---- log out: scenes hold the student's open code, exam and quiz --
        if (app.signOutRequested) {
            app.signOutRequested = false;
            for (auto& s : gScenes) s.reset();   // flushes unsaved code to the student's folder
            SignOut();
            CreateScenes();
        }

        // ---- scene switching ------------------------------------------
        if (app.switchRequested) {
            app.switchRequested = false;
            app.current = app.pending;
            app.currentParam = app.pendingParam;
            ClearTextFocus();
            SetInputEnabled(true);
            gScenes[(int)app.current]->Enter(app.pendingParam);
            fade = 1;
        }
        Scene* scene = gScenes[(int)app.current].get();
        float dt = std::fmin(GetFrameTime(), 0.05f);

        // ---- draw everything into the canvas ----------------------------
        BeginTextureMode(canvas);
        ClearBackground(col::bg);
        Camera2D cam = {};
        cam.zoom = scale;
        BeginMode2D(cam);
        ui::BeginFrame();
        Backdrop((float)GetTime());
        const bool pickerWasOpen = app.themePickerOpen;
        if (pickerWasOpen) SetInputEnabled(false);
        scene->Frame(dt);
        if (fade > 0) {
            DrawRectangleRec({0, 64, W, H - 64}, Alpha(col::bg, fade));
            fade = std::fmax(0.0f, fade - dt * 7.0f);
        }
        SetInputEnabled(!pickerWasOpen);
        bool back = TopBar(scene);
        if (pickerWasOpen) ThemePicker();
        if ((back || (IsKeyPressed(KEY_ESCAPE) && !AnyTextFocused() && !pickerWasOpen)) && app.current != SceneId::Home && app.current != SceneId::Login) {
            if (app.hasReturn) {
                SceneId to = app.returnScene;
                std::string param = app.returnParam;
                app.Go(to, param);
            } else app.Go(SceneId::Home);
        }
        if (IsKeyPressed(KEY_ESCAPE)) ClearTextFocus();
        DrawToasts(dt);
        EndMode2D();
        EndTextureMode();

        // ---- present ----------------------------------------------------
        BeginDrawing();
        ClearBackground(col::bg);
        // copy the canvas as-is (its colours are already final; ignore its alpha)
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM);
        DrawTexturePro(canvas.texture, {0, 0, (float)canvas.texture.width, -(float)canvas.texture.height},
                       {offX, offY, dw, dh}, {0, 0}, 0, WHITE);
        EndBlendMode();
        EndDrawing();
        SetMouseCursor(WantsIBeam() ? MOUSE_CURSOR_IBEAM : WantsHandCursor() ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);

        frame++;
        if (bench) {
            double now = GetTime();
            if (frame > 1) {
                double ms = (now - benchLast) * 1000.0;
                if (ms > benchWorst) benchWorst = ms;
                if (ms > 1000.0 / 60) benchSlow++;
            }
            benchLast = now;
            // tour: a new screen every 12 frames, as a student clicking around would
            if (benchTour && frame % 12 == 0) {
                static const SceneId tour[] = {SceneId::Home, SceneId::Study, SceneId::Practice, SceneId::Mock,
                                               SceneId::Quiz, SceneId::Visualizer, SceneId::ListLab, SceneId::Grid, SceneId::Login};
                int k = (frame / 12) % 9;
                app.Go(tour[k], tour[k] == SceneId::Practice && (frame / 108) % 2 ? "isLeapYear" : "");
                if (frame / 12 % 4 == 3) app.themePickerOpen = !app.themePickerOpen;
            }
        }
        if (bench && frame == 30) benchStart = GetTime();   // after fonts and caches warm up
        if (bench && frame == shotFrame && frame > 30)
            printf("bench: %.2f ms/frame over %d frames, worst %.1f ms, %d frames over 16.7 ms, startup %.2f s, window %dx%d, canvas %dx%d\n",
                   (GetTime() - benchStart) * 1000.0 / (frame - 30), frame - 30, benchWorst, benchSlow, benchStart,
                   GetScreenWidth(), GetScreenHeight(), canvas.texture.width, canvas.texture.height), fflush(stdout);
        if (shotFile && frame == shotFrame) {
            Image img = LoadImageFromTexture(canvas.texture);
            ImageFlipVertical(&img);
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
            ExportImage(img, shotFile);
            UnloadImage(img);
            break;
        }
    }

    for (auto& s : gScenes) s.reset();
    if (canvas.id) UnloadRenderTexture(canvas);
    UnloadFonts();
    CloseWindow();
    return 0;
}
