// ============================================================================
// screencast.cpp  -  the screen recorder (see screencast.h).
//
// The main thread reads the canvas back from the GPU at 30 frames a second of
// real time (a slow frame is repeated, so the video keeps the right speed) and
// queues it; a worker thread pipes the frames to ffmpeg, which encodes H.264.
// If the encoder falls behind, frames are dropped rather than stalling the app.
// ============================================================================
#include "app/screencast.h"
#include "app/app.h"
#include "app/process.h"
#include "app/ui.h"
#include "rlgl.h"
#include <algorithm>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace ui;

namespace rec {
namespace {

const int kFps = 30;
const size_t kMaxQueued = 6;   // ~50 MB at 1080p; more than that and frames are dropped

struct Frame {
    std::shared_ptr<std::vector<unsigned char>> pixels;
    int repeat;   // written this many times (the app was slower than 30 fps)
};

std::string gFfmpeg;            // its path, "" if not found
bool gLooked = false;
bool gRecording = false;
double gStart = 0;
int gFramesTaken = 0;           // frames of video so far (repeats included)
int gW = 0, gH = 0;
std::string gFile, gLastFile;

// shared with the worker
std::mutex gMu;
std::condition_variable gCv;
std::deque<Frame> gQueue;
bool gClosing = false;          // no more frames: finish the file
bool gFailed = false;           // ffmpeg went away
int gDropped = 0;
std::thread gWorker;
PipeProc* gProc = nullptr;

// results the worker leaves for the main thread to announce
bool gDone = false;
int gExit = 0;

void FindFfmpeg() {
    if (gLooked) return;
    gLooked = true;
#ifdef _WIN32
    const std::string exe = "ffmpeg.exe";
#else
    const std::string exe = "ffmpeg";
#endif
    std::error_code ec;
    std::string local = app.dataDir + exe;   // a copy next to the app wins
    if (std::filesystem::is_regular_file(local, ec)) gFfmpeg = local;
    else gFfmpeg = FindInPath(exe, GetEnvVar("PATH"));
}

std::string Stamp() {
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "SegFault_%Y-%m-%d_%H-%M-%S", &tm);
    return buf;
}

void Work() {
    for (;;) {
        Frame f;
        {
            std::unique_lock<std::mutex> lock(gMu);
            gCv.wait(lock, [] { return !gQueue.empty() || gClosing; });
            if (gQueue.empty()) break;   // closing, and everything is written
            f = gQueue.front();
            gQueue.pop_front();
        }
        for (int i = 0; i < f.repeat && !gFailed; i++)
            if (!PipeWrite(gProc, f.pixels->data(), f.pixels->size())) gFailed = true;
    }
    int code = PipeClose(gProc, 20000);   // ffmpeg writes the end of the file
    gProc = nullptr;
    std::lock_guard<std::mutex> lock(gMu);
    gExit = code;
    gDone = true;
}

void Start(int w, int h) {
    FindFfmpeg();
    if (gFfmpeg.empty()) {
        TraceLog(LOG_WARNING, "REC: ffmpeg not found");
#ifdef _WIN32
        Toast("Recording needs ffmpeg: run  winget install ffmpeg  then restart SegFault", col::warn);
#else
        Toast("Recording needs ffmpeg: install it with your package manager", col::warn);
#endif
        return;
    }
    if (gWorker.joinable()) return;   // the last video is still being finished
    std::error_code ec;
    std::filesystem::create_directories(Folder(), ec);
    gW = w;
    gH = h;
    gFile = Folder() + Stamp() + ".mp4";
    const std::string size = std::to_string(w) + "x" + std::to_string(h);
    // the canvas comes up bottom-first (OpenGL), and H.264 needs even sizes
    gProc = StartPipeProcess({gFfmpeg, "-hide_banner", "-loglevel", "error", "-y",
                              "-f", "rawvideo", "-pix_fmt", "rgba", "-s", size, "-framerate", std::to_string(kFps), "-i", "-",
                              "-vf", "vflip,scale=trunc(iw/2)*2:trunc(ih/2)*2",
                              "-c:v", "libx264", "-preset", "veryfast", "-crf", "20", "-pix_fmt", "yuv420p",
                              "-movflags", "+faststart", gFile},
                             "");
    if (!gProc) { Toast("Couldn't start ffmpeg", col::danger); TraceLog(LOG_WARNING, "REC: couldn't start %s", gFfmpeg.c_str()); return; }
    TraceLog(LOG_INFO, "REC: recording %dx%d to %s", w, h, gFile.c_str());
    gQueue.clear();
    gClosing = gFailed = gDone = false;
    gDropped = 0;
    gFramesTaken = 0;
    gStart = GetTime();
    gRecording = true;
    gWorker = std::thread(Work);
}

} // namespace

bool Available() { FindFfmpeg(); return !gFfmpeg.empty(); }
bool Recording() { return gRecording; }
float Seconds() { return gRecording ? (float)(GetTime() - gStart) : 0; }
std::string Folder() { return app.dataDir + "segfault_recordings/"; }
std::string LastFile() { return gLastFile; }

void Stop() {
    if (!gRecording) return;
    gRecording = false;
    {
        std::lock_guard<std::mutex> lock(gMu);
        gClosing = true;
    }
    gCv.notify_all();
    Toast("Saving the video...", col::accent);
}

static bool gStartWanted = false;   // the next Capture starts (it knows the canvas size)

void Toggle() {
    if (gRecording) Stop();
    else gStartWanted = true;
}

// A finished worker: join it and say how it went.
static void Collect() {
    bool done;
    {
        std::lock_guard<std::mutex> lock(gMu);
        done = gDone;
    }
    if (!done || !gWorker.joinable()) return;
    gWorker.join();
    gDone = false;
    std::error_code ec;
    bool ok = gExit == 0 && std::filesystem::exists(gFile, ec) && std::filesystem::file_size(gFile, ec) > 0;
    if (ok) {
        gLastFile = gFile;
        std::string name = std::filesystem::path(gFile).filename().string();
        Toast("Video saved: segfault_recordings/" + name + (gDropped ? "  (" + std::to_string(gDropped) + " frames skipped)" : ""), col::success);
    } else Toast("The video couldn't be saved (ffmpeg stopped with an error)", col::danger);
}

void Capture(const RenderTexture2D& canvas) {
    Collect();
    if (gStartWanted) {
        gStartWanted = false;
        Start(canvas.texture.width, canvas.texture.height);
    }
    if (!gRecording) return;
    if (gFailed) { Stop(); return; }
    if (canvas.texture.width != gW || canvas.texture.height != gH) {   // the window changed size: this video ends here
        Stop();
        return;
    }
    int want = (int)((GetTime() - gStart) * kFps) + 1;
    int n = want - gFramesTaken;
    if (n <= 0) return;   // faster than 30 fps: this frame isn't needed
    gFramesTaken = want;
    unsigned char* px = (unsigned char*)rlReadTexturePixels(canvas.texture.id, gW, gH, canvas.texture.format);
    if (!px) return;
    auto buf = std::make_shared<std::vector<unsigned char>>(px, px + (size_t)gW * gH * 4);
    RL_FREE(px);
    {
        std::lock_guard<std::mutex> lock(gMu);
        if (gQueue.size() >= kMaxQueued) gDropped += n;
        else gQueue.push_back({buf, std::min(n, kFps)});
    }
    gCv.notify_one();
}

void DrawBadge() {
    if (!gRecording) return;
    float s = Seconds();
    int sec = (int)s;
    std::string label = TextFormat("REC  %d:%02d", sec / 60, sec % 60);
    float w = Measure(label, 13.5f, F_MONOBOLD).x + Measure("F9 to stop", 12, F_MEDIUM).x + 58;
    Rectangle r = {W / 2 - w / 2, 10, w, 30};
    bool hover = MouseIn(r);
    RoundRect(r, 15, Alpha(Mix(col::bg, BLACK, 0.4f), 0.88f));
    RoundRectLines(r, 15, 1.2f, Alpha(col::danger, hover ? 0.9f : 0.55f));
    float blink = 0.55f + 0.45f * std::sin((float)GetTime() * 5);
    DrawCircleV({r.x + 16, r.y + 15}, 5.5f, Alpha(col::danger, blink));
    Text(label, r.x + 28, r.y + 7, 13.5f, col::heading, F_MONOBOLD);
    Text(hover ? "click to stop" : "F9 to stop", r.x + 28 + Measure(label, 13.5f, F_MONOBOLD).x + 12, r.y + 8, 12, col::dim, F_MEDIUM);
    if (hover) WantHand();
    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) Stop();
}

void OpenFolder() {
    std::error_code ec;
    std::filesystem::create_directories(Folder(), ec);
    std::string dir = std::filesystem::path(Folder()).make_preferred().string();
#ifdef _WIN32
    RunProcess({"explorer", dir}, "", 5000);
#elif defined(__APPLE__)
    RunProcess({"open", dir}, "", 5000);
#else
    RunProcess({"xdg-open", dir}, "", 5000);
#endif
}

void Shutdown() {
    Stop();
    if (!gWorker.joinable()) return;
    gWorker.join();   // the video is finished before the app closes
    std::error_code ec;
    if (gExit == 0 && std::filesystem::exists(gFile, ec) && std::filesystem::file_size(gFile, ec) > 0) gLastFile = gFile;
}

} // namespace rec
