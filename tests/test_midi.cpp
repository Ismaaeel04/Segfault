// midi_tests: reading Standard MIDI Files (src/app/midi.h).
#include "../src/app/midi.h"
#include <cmath>
#include <cstdio>

static int fails = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; std::printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)
static bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

typedef std::vector<unsigned char> Bytes;
static void Put32(Bytes& b, unsigned v) { for (int s = 24; s >= 0; s -= 8) b.push_back((unsigned char)(v >> s)); }
static void Track(Bytes& file, const Bytes& body) {
    file.insert(file.end(), {'M', 'T', 'r', 'k'});
    Put32(file, (unsigned)body.size());
    file.insert(file.end(), body.begin(), body.end());
}

int main() {
    // format 1, 2 tracks, 480 ticks a beat
    Bytes f = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0x01, 0xE0};
    // track 0: tempo 600000 us a beat (100 bpm) at tick 0, then 50 bpm at tick 960; end at 1920
    Track(f, {0x00, 0xFF, 0x51, 0x03, 0x09, 0x27, 0xC0,
              0x87, 0x40, 0xFF, 0x51, 0x03, 0x12, 0x4F, 0x80,   // delta 960
              0x87, 0x40, 0xFF, 0x2F, 0x00});                   // delta 960
    // track 1: program 80 on channel 1; C4 0..480; E4 480..960 (running status, velocity 0 = off);
    // G4 1440..1920 (in the slower tempo); A4 at 1920, never released
    Track(f, {0x00, 0xC1, 0x50,
              0x00, 0x91, 60, 100,
              0x83, 0x60, 0x81, 60, 0,       // delta 480: note off
              0x00, 0x91, 64, 90,
              0x83, 0x60, 64, 0,             // running status 0x91, velocity 0
              0x83, 0x60, 67, 80,            // delta 480: tick 1440
              0x83, 0x60, 67, 0,
              0x00, 69, 70,
              0x00, 0xFF, 0x2F, 0x00});
    midi::Song s = midi::Parse(f);
    CHECK(s.ok);
    CHECK(s.notes.size() == 4);
    if (s.notes.size() == 4) {
        const double beat100 = 0.6, beat50 = 1.2;
        CHECK(s.notes[0].key == 60 && Near(s.notes[0].start, 0) && Near(s.notes[0].length, beat100));
        CHECK(s.notes[0].program == 80 && s.notes[0].channel == 1 && s.notes[0].velocity == 100);
        CHECK(s.notes[1].key == 64 && Near(s.notes[1].start, beat100) && Near(s.notes[1].length, beat100));
        // G4 on at tick 1440 (= 2 beats at 100 bpm + 1 beat at 50 bpm), off at 1920
        CHECK(s.notes[2].key == 67 && Near(s.notes[2].start, 2 * beat100 + beat50) && Near(s.notes[2].length, beat50));
        CHECK(s.notes[3].key == 69);                       // never released: closed at the end
        CHECK(Near(s.end, 2 * beat100 + 2 * beat50));          // the end-of-track, in seconds
        CHECK(s.length >= s.end);
    }
    // broken files are refused, not half-read
    CHECK(!midi::Parse(Bytes{}).ok);
    CHECK(!midi::Parse(Bytes{'R', 'I', 'F', 'F', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96}).ok);
    Bytes cut = f;
    cut.resize(cut.size() - 9);
    CHECK(!midi::Parse(cut).ok);
    Bytes smpte = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 0, 0xE7, 0x28};
    CHECK(!midi::Parse(smpte).ok);
    std::printf("midi: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
