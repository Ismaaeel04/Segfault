// ============================================================================
// midi.h  -  Reads a Standard MIDI File (format 0 or 1) into a list of notes
// with their times in seconds, so the game's chiptune synthesiser can play it.
// Handles running status, tempo changes, note-on with velocity 0 as note-off,
// program changes and meta/sysex events. No raylib here, so midi_tests can test it.
// ============================================================================
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace midi {

struct Note {
    double start = 0, length = 0;   // seconds
    int channel = 0, key = 60, velocity = 100, program = 0;
};

struct Song {
    std::vector<Note> notes;
    double length = 0;              // seconds, until the last sound stops
    double end = 0;                 // seconds, at the last end-of-track (where a loop starts again)
    bool ok = false;
    std::string error;
};

namespace detail {
struct Reader {
    const std::vector<unsigned char>& d;
    size_t i = 0;
    bool bad = false;
    explicit Reader(const std::vector<unsigned char>& data) : d(data) {}
    unsigned U8() { if (i >= d.size()) { bad = true; return 0; } return d[i++]; }
    unsigned U16() { unsigned a = U8(); return (a << 8) | U8(); }
    unsigned U32() { unsigned a = U16(); return (a << 16) | U16(); }
    unsigned Vlq() {
        unsigned v = 0;
        for (int k = 0; k < 4; k++) { unsigned b = U8(); v = (v << 7) | (b & 0x7F); if (!(b & 0x80)) return v; }
        bad = true;
        return v;
    }
};
struct Raw { uint64_t tick; int type; int ch, a, b; uint32_t tempo; };   // type: 0 on, 1 off, 2 program, 3 tempo, 4 end
} // namespace detail

inline Song Parse(const std::vector<unsigned char>& data) {
    using namespace detail;
    Song song;
    Reader r(data);
    if (data.size() < 14 || r.U32() != 0x4D546864u) { song.error = "not a MIDI file"; return song; }   // "MThd"
    unsigned hlen = r.U32();
    unsigned format = r.U16(), tracks = r.U16(), division = r.U16();
    r.i = 8 + hlen;
    if (format > 1) { song.error = "only MIDI formats 0 and 1"; return song; }
    if (division & 0x8000) { song.error = "SMPTE timing is not supported"; return song; }
    std::vector<Raw> ev;
    for (unsigned t = 0; t < tracks && !r.bad; t++) {
        if (r.i + 8 > data.size() || r.U32() != 0x4D54726Bu) { song.error = "a track is missing"; return song; }   // "MTrk"
        size_t end = r.i + 4;
        end += r.U32();
        if (end > data.size()) { song.error = "a track is cut short"; return song; }
        uint64_t tick = 0;
        int status = 0;
        while (r.i < end && !r.bad) {
            tick += r.Vlq();
            unsigned b = r.U8();
            if (b == 0xFF) {   // meta
                unsigned type = r.U8(), len = r.Vlq();
                size_t at = r.i;
                if (type == 0x51 && len == 3) ev.push_back({tick, 3, 0, 0, 0, (uint32_t)((data[at] << 16) | (data[at + 1] << 8) | data[at + 2])});
                if (type == 0x2F) ev.push_back({tick, 4, 0, 0, 0, 0});
                r.i = at + len;
                continue;
            }
            if (b == 0xF0 || b == 0xF7) { unsigned len = r.Vlq(); r.i += len; continue; }   // sysex
            if (b & 0x80) status = (int)b;
            else r.i--;   // running status: this byte is data
            if (!status) { song.error = "data before any status"; return song; }
            int hi = status & 0xF0, ch = status & 0x0F;
            if (hi == 0xC0 || hi == 0xD0) {
                int a = (int)r.U8();
                if (hi == 0xC0) ev.push_back({tick, 2, ch, a, 0, 0});
                continue;
            }
            int a = (int)r.U8(), v = (int)r.U8();
            if (hi == 0x90 && v > 0) ev.push_back({tick, 0, ch, a, v, 0});
            else if (hi == 0x80 || hi == 0x90) ev.push_back({tick, 1, ch, a, 0, 0});
        }
        r.i = end;
    }
    if (r.bad) { song.error = "the file is cut short"; return song; }
    // all tracks on one timeline; at the same tick: tempo and programs first, offs before ons
    std::stable_sort(ev.begin(), ev.end(), [](const Raw& x, const Raw& y) {
        if (x.tick != y.tick) return x.tick < y.tick;
        auto rank = [](int t) { return t == 3 ? 0 : t == 2 ? 1 : t == 1 ? 2 : t == 0 ? 3 : 4; };
        return rank(x.type) < rank(y.type);
    });
    double sec = 0, secPerTick = 0.5 / division;   // 120 bpm until a tempo says otherwise
    uint64_t last = 0;
    int program[16] = {};
    struct Open { int ch, key, vel, prog; double start; };
    std::vector<Open> open;
    for (const Raw& e : ev) {
        sec += (double)(e.tick - last) * secPerTick;
        last = e.tick;
        switch (e.type) {
            case 3: secPerTick = e.tempo / 1e6 / division; break;
            case 2: program[e.ch] = e.a; break;
            case 0: open.push_back({e.ch, e.a, e.b, program[e.ch], sec}); break;
            case 1:
                for (size_t k = 0; k < open.size(); k++)
                    if (open[k].ch == e.ch && open[k].key == e.a) {
                        song.notes.push_back({open[k].start, std::max(0.01, sec - open[k].start), e.ch, e.a, open[k].vel, open[k].prog});
                        open.erase(open.begin() + (long)k);
                        break;
                    }
                break;
            case 4: song.end = std::max(song.end, sec); song.length = std::max(song.length, sec); break;
        }
    }
    for (const Open& o : open) song.notes.push_back({o.start, std::max(0.05, sec - o.start), o.ch, o.key, o.vel, o.prog});   // never released
    for (const Note& n : song.notes) song.length = std::max(song.length, n.start + n.length);
    if (song.end <= 0) song.end = song.length;
    std::sort(song.notes.begin(), song.notes.end(), [](const Note& x, const Note& y) { return x.start < y.start; });
    song.ok = true;
    return song;
}

} // namespace midi
