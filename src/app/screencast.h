// ============================================================================
// screencast.h  -  SegFault OS's screen recorder. F9 (or the start menu)
// starts and stops it; what the app shows is saved as an MP4 video in
// segfault_recordings/, next to the app. The frames are taken from the app's
// own canvas, so the video is pixel-exact and the REC badge is not in it.
// It needs ffmpeg (in PATH or next to the app) to encode the video.
// ============================================================================
#pragma once
#include "raylib.h"
#include <string>

namespace rec {

bool Available();          // ffmpeg was found
bool Recording();
float Seconds();           // how long the current recording is
void Toggle();             // start, or stop and save
void Stop();

// Every frame, once the canvas holds the finished picture.
void Capture(const RenderTexture2D& canvas);
// Every frame, drawn into the canvas after Capture (so it isn't recorded):
// the red REC badge (a click stops), and messages when a video is saved.
void DrawBadge();

std::string Folder();      // where the videos go (with a trailing slash)
void OpenFolder();         // in the system's file manager
std::string LastFile();    // the last video saved, or ""

void Shutdown();           // on exit: finishes the video being recorded

} // namespace rec
