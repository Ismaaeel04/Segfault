// ============================================================================
// sound.h  -  SegFault OS's sounds and The Forgotten Pointer's music, made in
// code: the system sounds (start-up, sign in/out, opening an app, shutting
// down...), chiptune effects for the game, and the game's songs - MIDI files
// played by a little 8-bit synthesiser with a lo-fi finish.
// ============================================================================
#pragma once
#include <string>

namespace snd {

enum Ui { CLICK, OPEN, CLOSE, STARTUP, LOGON, LOGOFF, SHUTDOWN, ERROR_SOUND, UI_COUNT };

// The game's effects.
enum Fx {
    FX_SWING, FX_HIT, FX_CRIT, FX_KILL, FX_HURT, FX_JUMP,       // fighting
    FX_QUIZ, FX_RIGHT, FX_WRONG,                                // questions
    FX_ROAR, FX_SPIT, FX_BOSS_HIT, FX_BOSS_DOWN,                // the boss
    FX_TERMINAL, FX_COMPILE_ERROR,                              // the boss's terminal
    FX_MENU_MOVE, FX_MENU_SELECT, FX_VICTORY,                   // menus, a chapter cleared
    FX_HOP, FX_GOLEM_WAKE, FX_SLAM, FX_ROCK_FALL,               // the Array Canyon: imps hopping, the Golem
    FX_COUNT
};

// After InitWindow. musicDir holds the .mid files. Does nothing with SEGFAULT_MUTE.
void Init(const std::string& musicDir);
void Update();                      // every frame: keeps the music playing
void Play(Ui sound);                // a system sound (if they are on)
void PlayFx(Fx effect, float volume);
void SetSystemSounds(bool on);
bool SystemSounds();

// ---- music: songs by id ("title", "story", "plains", "boss", "canyon", "golem", "gameover") ----
void Preload(const char* id);       // renders a song in the background, ready for later
void PlayMusic(const char* id);     // fades over from what is playing; loops (but "gameover" plays once)
void StopMusic();                   // fades out
void SetMusicVolume(float v);       // 0..1

// --render-sounds <dir>: writes the songs, effects and system sounds as .wav files.
bool RenderToFiles(const std::string& musicDir, const std::string& outDir);

// Tests: SEGFAULT_SOUND_TEST runs the sound code with the volume at zero, and
// this says what the music player is doing ("plains playing, rendered, ...").
std::string State();

} // namespace snd
