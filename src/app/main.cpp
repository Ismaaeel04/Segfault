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
static bool TopBar(Scene* scene) {
    DrawRectangleRec({0, 0, W, 64}, col::bg2);
    DrawRectangleRec({0, 63, W, 1}, Alpha(col::border, 0.9f));
    bool back = false;
    if (app.current == SceneId::Home) {
        RoundRect({24, 15, 34, 34}, 9, col::accent);
        RoundRect({33, 24, 16, 16}, 3, col::bg2);
        DrawCircleV({41, 32}, 3.5f, col::pointer);
        Text("Segfault", 70, 13, 22, WHITE, F_DISPLAY);
        Text("Learn C & C++ by writing it", 71, 40, 13, col::dim, F_MEDIUM);
    } else {
        back = Button({20, 13, 104, 38}, app.hasReturn ? "<  Back" : "<  Home", BTN_GHOST, true, 15);
        Text(scene->Title(), 142, 19, 21, WHITE, F_HEAVY);
    }
    const Progress& p = app.progress;
    float bx = W - 252;
    RoundRect({bx, 15, 228, 34}, 17, col::panel);
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
    printf("selftest: %zu problems, %zu lessons, %zu questions, %zu programs - %d failure(s)\n",
           Problems().size(), Lessons().size(), Questions().size(), MemPrograms().size(), fails);
    return fails ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--selftest") == 0) return SelfTest(argc >= 3 && std::strcmp(argv[2], "quick") == 0);

    // Optional: --shot <scene> <param> <file.png> renders one screen and exits.
    const char* shotFile = nullptr;
    int shotScene = -1;
    std::string shotParam;
    if (argc >= 5 && std::strcmp(argv[1], "--shot") == 0) {
        shotScene = std::atoi(argv[2]);
        shotParam = argv[3];
        if (shotParam == "-") shotParam.clear();
        shotFile = argv[4];
    }

    unsigned flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT;
#ifdef __APPLE__
    flags |= FLAG_WINDOW_HIGHDPI;     // sharp text on Retina screens
#endif
    SetConfigFlags(flags);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow((int)W, (int)H, "Segfault - Learn C & C++");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    SetWindowMinSize(800, 500);
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
    if (!shotFile) app.progress.Load(app.dataDir + "segfault_progress.txt");

    gScenes[(int)SceneId::Home].reset(CreateHomeScene());
    gScenes[(int)SceneId::Study].reset(CreateStudyScene());
    gScenes[(int)SceneId::Practice].reset(CreatePracticeScene());
    gScenes[(int)SceneId::Mock].reset(CreateMockScene());
    gScenes[(int)SceneId::Quiz].reset(CreateQuizScene());
    gScenes[(int)SceneId::Visualizer].reset(CreateVisualizerScene());
    gScenes[(int)SceneId::ListLab].reset(CreateListLabScene());
    gScenes[(int)SceneId::Grid].reset(CreateGridScene());

    if (shotFile) app.Go((SceneId)shotScene, shotParam);
    else gScenes[0]->Enter("");

    RenderTexture2D canvas = {};
    float currentScale = -1;
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

        // ---- scene switching ------------------------------------------
        if (app.switchRequested) {
            app.switchRequested = false;
            app.current = app.pending;
            app.currentParam = app.pendingParam;
            ClearTextFocus();
            SetInputEnabled(true);
            gScenes[(int)app.current]->Enter(app.pendingParam);
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
        scene->Frame(dt);
        SetInputEnabled(true);
        bool back = TopBar(scene);
        if ((back || (IsKeyPressed(KEY_ESCAPE) && !AnyTextFocused())) && app.current != SceneId::Home) {
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
        if (shotFile && frame == 50) {
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
