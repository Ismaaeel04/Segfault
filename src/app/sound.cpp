// ============================================================================
// sound.cpp  -  SegFault OS's sounds and the game's music (see sound.h).
//
// Music: MIDI files (resources/music) played by a small chiptune synthesiser -
// pulse waves, a stepped triangle and LFSR noise, like an 8-bit console - with
// a lo-fi finish: a warm low-pass, a little tape wobble and vinyl crackle.
// Each song is rendered once, in a background thread, and streamed. A looping
// song is rendered "circularly": a note still ringing at its end carries on
// at its start, so the loop has no seam.
// Effects: short chiptune sounds for the game. System sounds: soft
// electric-piano motifs for SegFault OS.
// ============================================================================
#include "sound.h"
#include "midi.h"
#include "raylib.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

namespace snd {
namespace {

const int SR = 44100;
const float TAU = 6.28318530718f;

float Mtof(float m) { return 440.0f * std::pow(2.0f, (m - 69) / 12.0f); }

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1) {}
    uint32_t Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float U() { return (Next() & 0xFFFFFF) / 16777216.0f; }   // 0..1
    float S() { return U() * 2 - 1; }                          // -1..1
};

// A stereo buffer. A loop is circular: writing past the end wraps to the
// start. A one-shot (wrap = false) just drops what doesn't fit.
struct Buf {
    int n;
    std::vector<float> L, R;
    bool wrap = true;
    explicit Buf(int frames, bool circular = true) : n(std::max(1, frames)), L(n, 0), R(n, 0), wrap(circular) {}
    void Add(long i, float l, float r) {
        if (wrap) { i %= n; if (i < 0) i += n; }
        else if (i < 0 || i >= n) return;
        L[i] += l;
        R[i] += r;
    }
};

void Pan(float pan, float& gl, float& gr) { gl = std::sqrt(0.5f * (1 - pan)); gr = std::sqrt(0.5f * (1 + pan)); }

// Tape wobble: a slow drift and a little flutter, as on an old cassette.
float Wow(double T, float depth) { return 1 + depth * (0.0022f * std::sin(TAU * 0.29f * (float)T) + 0.0007f * std::sin(TAU * 5.3f * (float)T)); }

// ------------------------------------------------------- chip oscillators --
// A band-limited step (polyBLEP), so the pulse waves don't alias harshly.
float Blep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}
float Pulse(float p, float dt, float duty) {
    float v = p < duty ? 1.0f : -1.0f;
    v += Blep(p, dt);
    float q = p - duty;
    if (q < 0) q += 1;
    v -= Blep(q, dt);
    return v;
}
// The NES-style triangle: 16 steps, which gives it its slightly gritty tone.
float Triangle(float p) {
    float v = 4 * std::fabs(p - 0.5f) - 1;
    return std::round(v * 7.5f) / 7.5f;
}
// 15-bit LFSR noise, clocked at `rate` steps a second.
struct Noise {
    uint32_t reg = 1;
    float acc = 0, out = 1;
    float Next(float rate) {
        acc += rate / SR;
        while (acc >= 1) {
            acc -= 1;
            uint32_t fb = (reg ^ (reg >> 1)) & 1;
            reg = (reg >> 1) | (fb << 14);
            out = (reg & 1) ? 1.0f : -1.0f;
        }
        return out;
    }
};

// ------------------------------------------------------------ instruments --
enum Osc { W_PULSE, W_TRI, W_NOISE };   // (raylib already has a Wave)
struct Inst { Osc wave; float duty, a, d, s, r, vol; bool vibrato; bool chorus; };

// General MIDI programs used by the songs, as chip voices.
Inst ForProgram(int program) {
    switch (program) {
        case 80: return {W_PULSE, 0.50f, 0.004f, 0.12f, 0.60f, 0.07f, 0.24f, true, false};    // square lead
        case 81: return {W_PULSE, 0.25f, 0.004f, 0.10f, 0.62f, 0.06f, 0.22f, true, false};    // "saw" lead: a thin pulse
        case 82: return {W_PULSE, 0.125f, 0.010f, 0.20f, 0.50f, 0.10f, 0.18f, true, false};   // calliope
        case 38: return {W_TRI, 0.5f, 0.002f, 0.20f, 0.85f, 0.04f, 0.50f, false, false};      // synth bass: the triangle
        case 50: return {W_PULSE, 0.50f, 0.060f, 0.30f, 0.70f, 0.25f, 0.09f, false, true};    // synth strings: a soft pad
        case 62: return {W_PULSE, 0.25f, 0.020f, 0.12f, 0.70f, 0.08f, 0.15f, true, false};    // synth brass
        default: return {W_PULSE, 0.50f, 0.005f, 0.15f, 0.60f, 0.08f, 0.18f, true, false};
    }
}

float Adsr(const Inst& in, float t, float held) {
    float e;
    if (t < in.a) e = t / in.a;
    else if (t < in.a + in.d) e = 1 - (1 - in.s) * (t - in.a) / in.d;
    else e = in.s;
    if (t > held) e *= std::max(0.0f, 1 - (t - held) / in.r);
    return e;
}

void ChipNote(Buf& b, double start, float key, float held, float vel, const Inst& in, float pan, double songT0) {
    const float f = Mtof(key);
    float gl, gr;
    Pan(pan, gl, gr);
    long s0 = (long)std::lround(start * SR);
    const int len = (int)((held + in.r) * SR) + 1;
    float p = 0, p2 = 0.37f;
    for (int i = 0; i < len; i++) {
        float t = (float)i / SR;
        double T = songT0 + start + t;
        float vib = in.vibrato && t > 0.22f ? 1 + 0.0045f * std::min(1.0f, (t - 0.22f) * 4) * std::sin(TAU * 5.6f * t) : 1;
        float fr = f * vib * Wow(T, 1);
        float dt = fr / SR;
        p += dt; if (p >= 1) p -= 1;
        float v;
        if (in.wave == W_TRI) v = Triangle(p);
        else v = Pulse(p, dt, in.duty);
        if (in.chorus) {   // a second, slightly detuned voice: a wider pad
            float dt2 = fr * 1.004f / SR;
            p2 += dt2; if (p2 >= 1) p2 -= 1;
            v = 0.5f * (v + Pulse(p2, dt2, in.duty));
        }
        v *= Adsr(in, t, held) * vel * in.vol;
        b.Add(s0 + i, v * gl, v * gr);
    }
}

// Drums on MIDI channel 10, the General MIDI way: the key picks the drum.
void Drum(Buf& b, double start, int key, float vel, Noise& nz) {
    long s0 = (long)std::lround(start * SR);
    auto tone = [&](float f0, float f1, float len, float decay, float vol, float noiseMix, float noiseRate) {
        float p = 0;
        for (int i = 0; i < (int)(len * SR); i++) {
            float t = (float)i / SR;
            float f = f1 + (f0 - f1) * std::exp(-t * 28);
            p += f / SR; if (p >= 1) p -= 1;
            float v = (Triangle(p) * (1 - noiseMix) + nz.Next(noiseRate) * noiseMix) * std::exp(-t * decay) * vol * vel;
            b.Add(s0 + i, v * 0.7071f, v * 0.7071f);
        }
    };
    auto noise = [&](float rate, float len, float decay, float vol, float pan) {
        float gl, gr, prev = 0;
        Pan(pan, gl, gr);
        for (int i = 0; i < (int)(len * SR); i++) {
            float t = (float)i / SR;
            float n = nz.Next(rate);
            float hp = n - prev * 0.6f;
            prev = n;
            float v = hp * std::exp(-t * decay) * vol * vel;
            b.Add(s0 + i, v * gl, v * gr);
        }
    };
    switch (key) {
        case 35: case 36: tone(170, 48, 0.28f, 13, 0.62f, 0.05f, 4000); break;                 // kick
        case 38: case 40: case 39: tone(200, 170, 0.20f, 19, 0.40f, 0.72f, 14000); break;      // snare / clap
        case 37: noise(20000, 0.05f, 60, 0.18f, 0); break;                                     // side stick
        case 42: case 44: noise(30000, 0.05f, 75, 0.10f, 0.25f); break;                        // closed hi-hat
        case 46: noise(30000, 0.30f, 11, 0.08f, 0.25f); break;                                 // open hi-hat
        case 49: case 51: case 52: case 55: case 57: case 59: noise(26000, 0.90f, 5, 0.07f, -0.2f); break;   // cymbals
        case 41: case 43: case 45: case 47: case 48: case 50:                                  // toms
            tone(Mtof((float)key + 12) * 1.6f, Mtof((float)key + 12), 0.30f, 11, 0.40f, 0.1f, 6000); break;
        default: noise(16000, 0.08f, 40, 0.10f, 0); break;
    }
}

// Master: a warm low-pass, no DC, gentle saturation, normalised. For a loop the
// filters run twice round it, so their state is settled where it joins.
void Master(Buf& b, float cutoff, float peakTo) {
    const float a = 1 - std::exp(-TAU * cutoff / SR), hpA = 1 - std::exp(-TAU * 28.0f / SR);
    for (int ch = 0; ch < 2; ch++) {
        std::vector<float>& x = ch ? b.R : b.L;
        std::vector<float> out(b.n);
        float lp = 0, hp = 0;
        for (int pass = b.wrap ? 0 : 1; pass < 2; pass++)
            for (int i = 0; i < b.n; i++) {
                lp += (x[i] - lp) * a;
                hp += (lp - hp) * hpA;
                if (pass == 1) out[i] = std::tanh((lp - hp) * 1.2f);
            }
        x.swap(out);
    }
    float peak = 0.0001f;
    for (int i = 0; i < b.n; i++) peak = std::max(peak, std::max(std::fabs(b.L[i]), std::fabs(b.R[i])));
    float g = peakTo / peak;
    for (int i = 0; i < b.n; i++) { b.L[i] *= g; b.R[i] *= g; }
}

// Vinyl: a faint hiss and an occasional crackle.
void Vinyl(Buf& b, uint32_t seed) {
    Rng nz(seed);
    float hiss = 0, crack = 0;
    for (int i = 0; i < b.n; i++) {
        if (nz.U() < 3.0f / SR) crack = nz.S() * 0.05f;
        crack *= 0.92f;
        hiss += (nz.S() - hiss) * 0.08f;
        float v = crack + hiss * 0.006f;
        b.Add(i, v, v * 0.93f);
    }
}

Buf RenderSong(const midi::Song& s, bool loop) {
    double len = loop ? s.end : s.length + 1.2;
    Buf b((int)std::lround(len * SR), loop);
    Noise nz;
    const float chanPan[16] = {-0.20f, 0.25f, -0.35f, 0.35f, 0.10f, -0.10f, 0.15f, -0.25f, 0.20f, 0, 0, 0, 0, 0, 0, 0};
    for (const midi::Note& n : s.notes) {
        float vel = n.velocity / 127.0f;
        if (n.channel == 9) Drum(b, n.start, n.key, vel, nz);
        else ChipNote(b, n.start, (float)n.key, (float)n.length, vel, ForProgram(n.program), chanPan[n.channel & 15], 0);
    }
    Vinyl(b, (uint32_t)(s.notes.size() * 2654435761u));
    Master(b, 6800.0f, 0.80f);
    return b;
}

// ---------------------------------------------------------------- effects --
// A game effect: a few chip segments. Noise segments sweep their clock rate.
struct Seg { Osc wave; float f0, f1, start, dur, vol, duty; float vib; };

Buf RenderFx(const std::vector<Seg>& segs, float level) {
    float end = 0;
    for (const Seg& s : segs) end = std::max(end, s.start + s.dur);
    Buf b((int)((end + 0.15f) * SR), false);
    Noise nz;
    for (const Seg& s : segs) {
        long s0 = (long)(s.start * SR);
        float p = 0;
        for (int i = 0; i < (int)(s.dur * SR); i++) {
            float t = (float)i / SR, u = t / s.dur;
            float f = s.f0 * std::pow(s.f1 / s.f0, u);   // an exponential sweep
            if (s.vib > 0) f *= 1 + 0.06f * std::sin(TAU * s.vib * t);
            float dt = f / SR;
            p += dt; if (p >= 1) p -= 1;
            float v = s.wave == W_PULSE ? Pulse(p, dt, s.duty) : s.wave == W_TRI ? Triangle(p) : nz.Next(f);
            float env = std::min(1.0f, t / 0.003f) * std::pow(1 - u, 1.3f);
            v *= env * s.vol;
            b.Add(s0 + i, v * 0.7071f, v * 0.7071f);
        }
    }
    Master(b, 9000.0f, level);
    return b;
}

std::vector<Seg> FxDef(Fx f) {
    const Osc P = W_PULSE, T = W_TRI, N = W_NOISE;
    switch (f) {
        case FX_SWING: return {{N, 16000, 5000, 0, 0.11f, 0.5f, 0, 0}, {P, 900, 300, 0, 0.08f, 0.12f, 0.125f, 0}};
        case FX_HIT: return {{P, 420, 150, 0, 0.07f, 0.8f, 0.125f, 0}, {N, 9000, 4000, 0, 0.05f, 0.5f, 0, 0}};
        case FX_CRIT: return {{P, 523, 523, 0, 0.05f, 0.7f, 0.5f, 0}, {P, 1046, 1046, 0.05f, 0.10f, 0.7f, 0.5f, 0}, {N, 12000, 3000, 0, 0.09f, 0.5f, 0, 0}};
        case FX_KILL: return {{P, 700, 90, 0, 0.22f, 0.7f, 0.25f, 0}, {N, 7000, 1500, 0, 0.26f, 0.6f, 0, 0},
                              {T, 1318, 1318, 0.12f, 0.08f, 0.6f, 0, 0}, {T, 1760, 1760, 0.20f, 0.14f, 0.6f, 0, 0}};
        case FX_HURT: return {{P, 330, 110, 0, 0.30f, 0.8f, 0.5f, 18}, {N, 5000, 1500, 0, 0.06f, 0.5f, 0, 0}};
        case FX_JUMP: return {{P, 280, 640, 0, 0.11f, 0.6f, 0.25f, 0}};
        case FX_QUIZ: return {{T, 988, 988, 0, 0.12f, 0.8f, 0, 0}, {T, 1318, 1318, 0.10f, 0.28f, 0.8f, 0, 0}};
        case FX_RIGHT: return {{P, 523, 523, 0, 0.07f, 0.6f, 0.5f, 0}, {P, 659, 659, 0.06f, 0.07f, 0.6f, 0.5f, 0},
                               {P, 784, 784, 0.12f, 0.07f, 0.6f, 0.5f, 0}, {P, 1046, 1046, 0.18f, 0.22f, 0.6f, 0.5f, 0}};
        case FX_WRONG: return {{P, 233, 233, 0, 0.14f, 0.7f, 0.5f, 0}, {P, 185, 185, 0.14f, 0.28f, 0.7f, 0.5f, 0}};
        case FX_ROAR: return {{N, 3000, 700, 0, 0.95f, 0.7f, 0, 0}, {P, 98, 55, 0, 0.95f, 0.6f, 0.25f, 9}, {T, 55, 40, 0, 0.95f, 0.8f, 0, 0}};
        case FX_SPIT: return {{P, 1000, 280, 0, 0.14f, 0.6f, 0.125f, 0}, {N, 8000, 3000, 0, 0.06f, 0.3f, 0, 0}};
        case FX_BOSS_HIT: return {{T, 150, 45, 0, 0.32f, 1.0f, 0, 0}, {N, 5000, 1200, 0, 0.26f, 0.6f, 0, 0}, {P, 220, 110, 0, 0.12f, 0.5f, 0.5f, 0}};
        case FX_BOSS_DOWN: return {{N, 6000, 250, 0, 1.6f, 0.8f, 0, 0}, {P, 700, 60, 0, 1.2f, 0.5f, 0.5f, 7}, {T, 110, 38, 0, 1.3f, 0.9f, 0, 0},
                                   {N, 3000, 400, 0.35f, 0.9f, 0.5f, 0, 0}};   // a big explosion; the fanfare is FX_VICTORY
        case FX_TERMINAL: return {{P, 1200, 1200, 0, 0.05f, 0.5f, 0.5f, 0}, {P, 1600, 1600, 0.07f, 0.07f, 0.5f, 0.5f, 0}};
        case FX_COMPILE_ERROR: return {{P, 110, 110, 0, 0.32f, 0.6f, 0.5f, 0}, {P, 116, 116, 0, 0.32f, 0.5f, 0.25f, 0}};
        case FX_MENU_MOVE: return {{P, 1100, 1100, 0, 0.03f, 0.5f, 0.5f, 0}};
        case FX_MENU_SELECT: return {{P, 660, 660, 0, 0.05f, 0.6f, 0.5f, 0}, {P, 990, 990, 0.05f, 0.08f, 0.6f, 0.5f, 0}};
        case FX_VICTORY: return {{P, 523, 523, 0, 0.12f, 0.6f, 0.5f, 0}, {P, 659, 659, 0.12f, 0.12f, 0.6f, 0.5f, 0},
                                 {P, 784, 784, 0.24f, 0.12f, 0.6f, 0.5f, 0}, {P, 1046, 1046, 0.36f, 0.12f, 0.6f, 0.5f, 0},
                                 {P, 1318, 1318, 0.48f, 0.12f, 0.6f, 0.5f, 0}, {P, 1568, 1568, 0.60f, 0.70f, 0.6f, 0.5f, 6},
                                 {T, 131, 131, 0, 0.36f, 0.8f, 0, 0}, {T, 196, 196, 0.36f, 0.36f, 0.8f, 0, 0}, {T, 262, 262, 0.72f, 0.6f, 0.8f, 0, 0}};
        default: return {};
    }
}

const float kFxLevel[FX_COUNT] = {0.35f, 0.55f, 0.60f, 0.60f, 0.60f, 0.32f, 0.50f, 0.50f, 0.50f, 0.70f, 0.35f, 0.70f, 0.75f, 0.35f, 0.50f, 0.22f, 0.35f, 0.60f};

// ------------------------------------------------------- system sounds ---
// A Rhodes-like electric piano for SegFault OS's own sounds.
void Rhodes(Buf& b, double start, float midi, float dur, float vel, float pan) {
    const float f = Mtof(midi);
    const float decay = 1.0f + f / 1000.0f;
    float gl, gr;
    Pan(pan, gl, gr);
    long s0 = (long)std::lround(start * SR);
    float ph = 0;
    for (int i = 0; i < (int)((dur + 3.0f) * SR); i++) {
        float t = (float)i / SR;
        ph += TAU * f / SR;
        float env = std::min(1.0f, t / 0.005f) * std::exp(-t * decay);
        if (t > dur) env *= std::exp(-(t - dur) * 7.0f);
        if (env < 0.00015f && t > 0.05f) break;
        float idx = 1.5f * std::exp(-t * 7.0f) + 0.22f;
        float v = std::sin(ph + idx * std::sin(ph)) + 0.11f * std::sin(ph * 7.0f) * std::exp(-t * 26.0f);
        v *= env * (1 - 0.13f * (0.5f + 0.5f * std::sin(TAU * 4.3f * t))) * vel;
        b.Add(s0 + i, v * gl, v * gr);
    }
}

Buf RenderUi(Ui u) {
    Buf b((int)(2.8f * SR), false);
    auto notes = [&](std::initializer_list<float> ms, float gap, float vel, float dur) {
        int k = 0;
        for (float m : ms) { Rhodes(b, 0.01 + k * gap, m, dur, vel, -0.3f + 0.2f * k); k++; }
    };
    switch (u) {
        case CLICK:
            for (int i = 0; i < (int)(0.03f * SR); i++) {
                float t = (float)i / SR, v = std::sin(TAU * 1750 * t) * std::exp(-t * 260) * 0.6f;
                b.Add(i + 40, v, v);
            }
            break;
        case OPEN: notes({76, 83}, 0.07f, 0.55f, 0.25f); break;
        case CLOSE: notes({83, 76}, 0.07f, 0.45f, 0.25f); break;
        case STARTUP: notes({60, 64, 67, 71, 74, 79}, 0.11f, 0.42f, 1.6f); break;
        case LOGON: notes({67, 74, 79}, 0.09f, 0.48f, 0.9f); break;
        case LOGOFF: notes({79, 74, 67}, 0.09f, 0.42f, 0.9f); break;
        case SHUTDOWN: notes({71, 67, 64, 60, 55}, 0.13f, 0.40f, 1.4f); break;
        case ERROR_SOUND: notes({52, 51}, 0.08f, 0.6f, 0.25f); break;
        default: break;
    }
    if (u != CLICK)   // a little echo
        for (int i = (int)(0.13f * SR); i < b.n; i++) {
            b.L[i] += 0.26f * b.R[i - (int)(0.13f * SR)];
            b.R[i] += 0.22f * b.L[i - (int)(0.11f * SR)];
        }
    float peak = 0.0001f;
    for (int i = 0; i < b.n; i++) peak = std::max(peak, std::max(std::fabs(b.L[i]), std::fabs(b.R[i])));
    const float level = u == CLICK ? 0.35f : u == ERROR_SOUND ? 0.5f : 0.55f;
    for (int i = 0; i < b.n; i++) {
        float fade = i > b.n - SR / 4 ? (float)(b.n - i) / (SR / 4) : 1;
        b.L[i] = b.L[i] / peak * level * fade;
        b.R[i] = b.R[i] / peak * level * fade;
    }
    return b;
}

std::vector<short> ToPcm(const Buf& b) {
    std::vector<short> pcm(b.n * 2);
    for (int i = 0; i < b.n; i++) {
        pcm[i * 2] = (short)(std::clamp(b.L[i], -1.0f, 1.0f) * 32767);
        pcm[i * 2 + 1] = (short)(std::clamp(b.R[i], -1.0f, 1.0f) * 32767);
    }
    return pcm;
}

Sound ToSound(const Buf& b) {
    std::vector<short> pcm = ToPcm(b);
    Wave w = {};
    w.frameCount = (unsigned)b.n;
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels = 2;
    w.data = pcm.data();
    return LoadSoundFromWave(w);   // copies the samples
}

// ------------------------------------------------------------------ songs --
// The songs the game uses so far (by id), from "15 Melodic RPG Chiptunes" by
// Aureolus_Omicron (CC0, opengameart.org).
struct SongDef { const char* id; const char* file; bool loop; };
const SongDef kSongs[] = {
    {"title", "rpgchip01_title_screen.mid", true},
    {"story", "rpgchip02_bittersweet_story.mid", true},
    {"plains", "rpgchip03_town.mid", true},
    {"boss", "rpgchip13_battle_1.mid", true},
    {"gameover", "rpgchip15_game_over.mid", false},
};
const int kSongCount = (int)(sizeof(kSongs) / sizeof(kSongs[0]));

std::string gMusicDir;

int SongIndex(const char* id) {
    for (int i = 0; i < kSongCount; i++) if (std::strcmp(kSongs[i].id, id) == 0) return i;
    return -1;
}

bool LoadMidi(int song, midi::Song& out) {
    std::ifstream in(gMusicDir + kSongs[song].file, std::ios::binary);
    if (!in) return false;
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    out = midi::Parse(data);
    return out.ok && !out.notes.empty();
}

// ------------------------------------------------------------------ state --
bool gReady = false, gSystem = true, gSilent = false;   // silent: tests run the code without a sound
Sound gUi[UI_COUNT] = {};
const int kAliases = 3;   // the same effect can overlap itself this many times
Sound gFx[FX_COUNT][kAliases] = {};
int gFxNext[FX_COUNT] = {};
bool gFxLoaded = false;

struct Rendered { std::atomic<bool> done{false}; bool failed = false; bool loop = true; std::vector<short> pcm; };
std::shared_ptr<Rendered> gSong[16];
AudioStream gStream = {};
bool gStreamOn = false, gEnded = false;
int gPlaying = -1, gWanted = -1;
size_t gPos = 0;
float gGain = 0, gVolume = 0.6f;
const int kChunk = 4096;

std::shared_ptr<Rendered> Request(int song) {
    if (!gSong[song]) {
        auto r = std::make_shared<Rendered>();
        r->loop = kSongs[song].loop;
        gSong[song] = r;
        midi::Song m;
        if (!LoadMidi(song, m)) { r->failed = true; r->done = true; return r; }
        bool loop = r->loop;
        std::thread([r, m, loop]() { r->pcm = ToPcm(RenderSong(m, loop)); r->done = true; }).detach();
    }
    return gSong[song];
}

void Feed() {
    std::shared_ptr<Rendered> r = gPlaying >= 0 ? gSong[gPlaying] : nullptr;
    static std::vector<short> chunk(kChunk * 2);
    while (IsAudioStreamProcessed(gStream)) {
        size_t frames = r && r->done && !r->failed ? r->pcm.size() / 2 : 0;
        for (int i = 0; i < kChunk; i++) {
            if (frames == 0 || gEnded) { chunk[i * 2] = chunk[i * 2 + 1] = 0; continue; }
            chunk[i * 2] = r->pcm[gPos * 2];
            chunk[i * 2 + 1] = r->pcm[gPos * 2 + 1];
            if (++gPos >= frames) { gPos = 0; if (!r->loop) gEnded = true; }   // a one-shot plays once
        }
        UpdateAudioStream(gStream, chunk.data(), kChunk);
    }
}

void LoadFx() {
    if (gFxLoaded) return;
    for (int f = 0; f < FX_COUNT; f++) {
        gFx[f][0] = ToSound(RenderFx(FxDef((Fx)f), kFxLevel[f]));
        for (int k = 1; k < kAliases; k++) gFx[f][k] = LoadSoundAlias(gFx[f][0]);
    }
    gFxLoaded = true;
}

} // namespace

void Init(const std::string& musicDir) {
    gMusicDir = musicDir;
    if (!gMusicDir.empty() && gMusicDir.back() != '/' && gMusicDir.back() != '\\') gMusicDir += '/';
    if (gReady || std::getenv("SEGFAULT_MUTE")) return;
    if (!IsAudioDeviceReady()) InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    gSilent = std::getenv("SEGFAULT_SOUND_TEST") != nullptr;
    for (int u = 0; u < UI_COUNT; u++) gUi[u] = ToSound(RenderUi((Ui)u));
    LoadFx();
    SetAudioStreamBufferSizeDefault(kChunk);
    gStream = LoadAudioStream(SR, 16, 2);
    gReady = true;
}

void Play(Ui u) {
    if (!gReady || !gSystem || gSilent || u < 0 || u >= UI_COUNT) return;
    PlaySound(gUi[u]);
}

void PlayFx(Fx f, float volume) {
    if (!gReady || gSilent || f < 0 || f >= FX_COUNT || volume <= 0) return;
    Sound& s = gFx[f][gFxNext[f]];
    gFxNext[f] = (gFxNext[f] + 1) % kAliases;
    SetSoundVolume(s, volume);
    PlaySound(s);
}

void SetSystemSounds(bool on) { gSystem = on; }
bool SystemSounds() { return gSystem; }

void Preload(const char* id) {
    int i = SongIndex(id);
    if (gReady && i >= 0) Request(i);
}

void PlayMusic(const char* id) {
    int i = SongIndex(id);
    if (!gReady || i < 0) { StopMusic(); return; }
    if (i == gWanted && i == gPlaying && !gEnded) return;   // already playing
    gWanted = i;
    Request(i);
    if (i == gPlaying) { gPos = 0; gEnded = false; }        // the same one-shot again: from the top
}

void StopMusic() { gWanted = -1; }
void SetMusicVolume(float v) { gVolume = std::clamp(v, 0.0f, 1.0f); }

void Update() {
    if (!gReady) return;
    float dt = std::min(GetFrameTime(), 0.05f);
    if (gWanted != gPlaying) {   // fade the old song out, then start the new one from its top
        gGain -= dt / 0.5f;
        if (gGain <= 0 || !gStreamOn) {
            gGain = 0;
            gPlaying = gWanted;
            gPos = 0;
            gEnded = false;
            if (gPlaying < 0 && gStreamOn) { StopAudioStream(gStream); gStreamOn = false; }
        }
    } else if (gPlaying >= 0 && gSong[gPlaying] && gSong[gPlaying]->done) {
        gGain = std::min(1.0f, gGain + dt / 0.8f);   // fade in
    }
    if (gPlaying >= 0 && !gStreamOn && gSong[gPlaying] && gSong[gPlaying]->done) { PlayAudioStream(gStream); gStreamOn = true; }
    if (gStreamOn) {
        SetAudioStreamVolume(gStream, gSilent ? 0.0f : gGain * gVolume);
        Feed();
    }
}

std::string State() {
    if (!gReady) return "audio off";
    std::string s = gPlaying >= 0 ? std::string(kSongs[gPlaying].id) : std::string("no song");
    s += gStreamOn ? " playing" : " stopped";
    if (gPlaying >= 0 && gSong[gPlaying])
        s += gSong[gPlaying]->failed ? ", MISSING FILE" : gSong[gPlaying]->done ? ", rendered" : ", rendering";
    s += ", position " + std::to_string(gPos) + ", gain " + std::to_string(gGain).substr(0, 4);
    return s;
}

bool RenderToFiles(const std::string& musicDir, const std::string& outDir) {
    gMusicDir = musicDir;
    if (!gMusicDir.empty() && gMusicDir.back() != '/' && gMusicDir.back() != '\\') gMusicDir += '/';
    auto save = [&](const Buf& b, const std::string& name) {
        std::vector<short> pcm = ToPcm(b);
        Wave w = {};
        w.frameCount = (unsigned)b.n;
        w.sampleRate = SR;
        w.sampleSize = 16;
        w.channels = 2;
        w.data = pcm.data();
        return ExportWave(w, (outDir + "/" + name + ".wav").c_str());
    };
    bool ok = true;
    for (int i = 0; i < kSongCount; i++) {
        midi::Song m;
        if (!LoadMidi(i, m)) { ok = false; continue; }
        Buf one = RenderSong(m, kSongs[i].loop);
        if (kSongs[i].loop) {   // twice round the loop, so the join can be heard
            Buf two(one.n * 2, false);
            for (int k = 0; k < two.n; k++) { two.L[k] = one.L[k % one.n]; two.R[k] = one.R[k % one.n]; }
            ok &= save(two, std::string("music_") + kSongs[i].id);
        } else ok &= save(one, std::string("music_") + kSongs[i].id);
    }
    const char* fxNames[FX_COUNT] = {"swing", "hit", "crit", "kill", "hurt", "jump", "quiz", "right", "wrong", "roar",
                                     "spit", "boss_hit", "boss_down", "terminal", "compile_error", "menu_move", "menu_select", "victory"};
    for (int f = 0; f < FX_COUNT; f++) ok &= save(RenderFx(FxDef((Fx)f), kFxLevel[f]), std::string("fx_") + fxNames[f]);
    const char* uiNames[UI_COUNT] = {"click", "open", "close", "startup", "logon", "logoff", "shutdown", "error"};
    for (int u = 0; u < UI_COUNT; u++) ok &= save(RenderUi((Ui)u), std::string("system_") + uiNames[u]);
    return ok;
}

} // namespace snd
