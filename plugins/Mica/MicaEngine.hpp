// Mica — 24-band stereo equalizer engine.
//   Zero Latency:  trapezoidal state-variable filters (TPT SVF cascades from the shared EQ band engine).
//   Natural Phase: the same filters plus a short FIR that restores the analog magnitude and phase near Nyquist
//                  (correction = analog response / digital response), a few milliseconds of latency.
//   Linear Phase:  FIR designed from the analog magnitude, FFT overlap-add.
// Dynamic EQ per band (internal or external sidechain detection), spectral dynamics (STFT, per-bin gain),
// Clean / Gentle / Warm character, auto gain, gain scale, phase invert, band solo, analyser feeds.
// Clean-room implementation from public, generic techniques (Zavalishin/Simper trapezoidal SVF, Butterworth and
// RBJ-style analog prototypes, frequency-sampling FIR design, partitioned convolution, STFT overlap-add).
// No third-party product code, assets or disassembly were used. License: MIT (see LICENSE).

#pragma once
#include "common/Convolver.hpp"
#include "common/EqBand.hpp"
#include "common/Fft.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mica {

using epsilon::cpx;
using epsilon::Fft;
using epsilon::Convolver;
namespace eq = epsilon::eq;

static constexpr float kPi = 3.14159265358979f;
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }

static constexpr int kNumBands = 24;
enum BandField { kBUsed, kBOn, kBShape, kBFreq, kBGain, kBQ, kBSlope, kBPlace, kBandFields };
// 2.0 per-band fields, in a second block after the 1.x parameters
enum Band2Field { kB2Slope, kB2Dyn, kB2Range, kB2Thresh, kB2Attack, kB2Release, kB2Source, kB2ScFree, kB2ScFreq, kB2ScQ, kB2Spectral, kBand2Fields };

// ------------------------------------------------------------------ params (append-only)
enum Param : uint32_t {
    kProcMode, kLinQuality, kChannelMode, kOutGain, kAnalyzer, kRange, kMidiOn,
    kBandFirst,
    kBandLast = kBandFirst + kNumBands * kBandFields - 1,
    kOutLearn,
    // ---- 2.0
    kCharacter, kAutoGain, kGainScale, kPhaseInvert,
    kAnaExternal, kAnaRange, kAnaSpeed, kAnaRes, kAnaTilt, kAnaFreeze, kCollision, kPiano,
    kBand2First,
    kBand2Last = kBand2First + kNumBands * kBand2Fields - 1,
    kParamCount
};
inline uint32_t bp(int band, int field) { return kBandFirst + band * kBandFields + field; }
inline uint32_t bp2(int band, int field) { return kBand2First + band * kBand2Fields + field; }

// shape numbers are stored in sessions: append only (same numbering as the shared engine)
enum Shape { kBell, kLowShelf, kHighShelf, kLowCut, kHighCut, kNotch, kBandPass, kTiltShelf, kFlatTilt, kAllPass, kNumShapes };
static_assert((int)kNumShapes == (int)eq::kNumShapes, "shape numbering");
enum Place { kPlaceBoth, kPlaceA, kPlaceB };              // A = left or mid, B = right or side
enum ProcMode { kZeroLatency, kLinearPhase, kNaturalPhase, kNumProcModes };
enum Character { kClean, kGentle, kWarm, kNumCharacters };

// slope settings: indices 0-3 are Mica 1.x (6, 12, 24, 48 dB/oct); the rest were added in 2.0
static constexpr int kNumSlopes = 17;                    // the last is Brickwall (cuts only)
inline int slopeDbOf(int idx) {
    static const int t[kNumSlopes] = { 6, 12, 24, 48, 18, 30, 36, 42, 54, 60, 66, 72, 78, 84, 90, 96, eq::kBrickwall };
    return t[clampi(idx, 0, kNumSlopes - 1)];
}
inline int slopeByRank(int r) {                          // r-th slope from shallow to steep
    static const int o[kNumSlopes] = { 0, 1, 4, 2, 5, 6, 7, 3, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    return o[clampi(r, 0, kNumSlopes - 1)];
}
inline int slopeRank(int idx) { for (int r = 0; r < kNumSlopes; ++r) if (slopeByRank(r) == idx) return r; return 1; }
inline bool isCutShape(int s) { return s == kLowCut || s == kHighCut; }
inline bool canDynamic(int s) { return eq::hasGain(s); }
// cuts keep their 1.x slope parameter; the other shapes use the 2.0 one (shelves were fixed at 12 dB/oct in 1.x)
inline uint32_t slopeParam(const float* p, int b) { return isCutShape((int)(p[bp(b, kBShape)] + 0.5f)) ? bp(b, kBSlope) : bp2(b, kB2Slope); }
inline int bandSlopeDb(const float* p, int b) {
    const int shape = (int)(p[bp(b, kBShape)] + 0.5f);
    const int idx = (int)(p[slopeParam(p, b)] + 0.5f);
    return isCutShape(shape) ? slopeDbOf(idx) : slopeDbOf(std::min(idx, kNumSlopes - 2));
}
inline float gainScaleOf(const float* p) { return p[kGainScale] / 100.f; }
inline eq::Spec bandSpec(const float* p, int b, float gainDb) {
    eq::Spec s; s.shape = clampi((int)(p[bp(b, kBShape)] + 0.5f), 0, kNumShapes - 1);
    s.freq = clampf(p[bp(b, kBFreq)], 10.f, 30000.f); s.gainDb = gainDb; s.q = clampf(p[bp(b, kBQ)], 0.025f, 40.f); s.slopeDb = bandSlopeDb(p, b);
    return s;
}
inline bool placeApplies(int place, int c) { return place == kPlaceBoth || (place == kPlaceA && c == 0) || (place == kPlaceB && c == 1); }
inline bool bandActive(const float* p, int b) { return p[bp(b, kBUsed)] > 0.5f && p[bp(b, kBOn)] > 0.5f; }
inline bool bandDynamic(const float* p, int b) { return p[bp2(b, kB2Dyn)] > 0.5f && canDynamic((int)(p[bp(b, kBShape)] + 0.5f)) && std::fabs(p[bp2(b, kB2Range)]) > 0.01f; }
inline bool bandSpectral(const float* p, int b) { return bandDynamic(p, b) && p[bp2(b, kB2Spectral)] > 0.5f; }

// dynamic gain computer: half a dB of change per dB over the threshold, up to the range, with a 6 dB soft knee
inline float dynChange(float levelDb, float thrDb, float rangeDb) {
    const float o = levelDb - thrDb;
    const float k = o <= -3.f ? 0.f : (o >= 3.f ? o : (o + 3.f) * (o + 3.f) / 12.f);
    const float c = std::min(std::fabs(rangeDb), 0.5f * k);
    return rangeDb < 0 ? -c : c;
}

// copyable atomics, so the engine itself can be copied (tests)
struct AtomicU32 {
    std::atomic<uint32_t> v { 0 };
    AtomicU32() {}
    AtomicU32(const AtomicU32& o) : v(o.v.load()) {}
    AtomicU32& operator=(const AtomicU32& o) { v.store(o.v.load()); return *this; }
    uint32_t load(std::memory_order m = std::memory_order_seq_cst) const { return v.load(m); }
    void store(uint32_t x, std::memory_order m = std::memory_order_seq_cst) { v.store(x, m); }
};
struct AtomicF {
    std::atomic<float> v { 0.f };
    AtomicF() {}
    AtomicF(const AtomicF& o) : v(o.v.load()) {}
    AtomicF& operator=(const AtomicF& o) { v.store(o.v.load()); return *this; }
    float load() const { return v.load(std::memory_order_relaxed); }
    void store(float x) { v.store(x, std::memory_order_relaxed); }
    float exchange(float x) { return v.exchange(x, std::memory_order_relaxed); }
};

// ------------------------------------------------------------------ one band
struct Band {
    // targets (from parameters)
    bool used = false, on = false; int shape = kBell, slope = 12, place = kPlaceBoth;
    // what the filter is currently built as (changes go through a short fade out / in)
    int curShape = kBell, curSlope = 12, curPlace = kPlaceBoth; bool swapping = false; int hold = 0;
    // smoothed controls
    float oct = 10, gain = 0, lq = 0, mix = 0, m = 0, dm = 0;   // mix: per control tick; m/dm: ramped per sample
    bool run = false;                                           // processed in the IIR path
    eq::Cascade<2> f;
    // dynamics
    bool dyn = false, spectral = false, external = false, detAll = false;
    eq::Cascade<2> det;
    float env = 0, aA = 0.01f, aR = 0.001f, dynDb = 0;
    float key[6] = { -1, -1, -1, -1, -1, -1 };              // the settings the filter was last designed for
    void reset() { f.reset(); det.reset(); env = 0; key[0] = -1; }
    bool applies(int c) const { return placeApplies(curPlace, c); }
};

// ------------------------------------------------------------------ spectral dynamics (STFT, per-bin gain)
class Spectral {
public:
    static constexpr int kMaxN = 4096;
    void init(float fs) {
        N = fs <= 50000.f ? 1024 : (fs <= 100000.f ? 2048 : 4096); H = N / 4; K = N / 2 + 1; sr = fs;
        fft.init(N);
        win.resize(N); for (int i = 0; i < N; ++i) win[i] = std::sqrt(0.5f - 0.5f * std::cos(2 * kPi * i / N));   // sqrt periodic Hann
        winLin = 0; for (int i = 0; i < N; ++i) winLin += win[i];
        for (int c = 0; c < 2; ++c) { in[c].assign(N, 0.f); sc[c].assign(N, 0.f); out[c].assign(N, 0.f); }
        buf.assign(N, cpx(0, 0)); for (int c = 0; c < 2; ++c) { X[c].assign(K, cpx(0, 0)); S[c].assign(K, cpx(0, 0)); }
        for (int b = 0; b < kNumBands; ++b) { g[b].assign(K, 0.f); w[b].assign(K, 0.f); wKey[b] = -1e9f; }
        gc[0].assign(K, 0.f); gc[1].assign(K, 0.f);
        show.assign(K, 0.f);
        t = 0;
    }
    int latency() const { return N; }
    int bins() const { return K; }
    float binHz(int k) const { return (float)k * sr / N; }
    void reset() { for (int c = 0; c < 2; ++c) { std::fill(in[c].begin(), in[c].end(), 0.f); std::fill(sc[c].begin(), sc[c].end(), 0.f); std::fill(out[c].begin(), out[c].end(), 0.f); }
                   for (int b = 0; b < kNumBands; ++b) std::fill(g[b].begin(), g[b].end(), 0.f);
                   t = 0; }

    // one stereo sample; y is replaced by the output delayed by latency()
    void process(float* y, const float* side, const float* p, const bool* active) {
        const int slot = (int)(t % (uint32_t)N);
        for (int c = 0; c < 2; ++c) { const float o = out[c][slot]; out[c][slot] = 0.f; in[c][slot] = y[c]; sc[c][slot] = side[c]; y[c] = o; }
        ++t;
        if (t % (uint32_t)H == 0) frame(slot, p, active);
    }
    std::vector<float> show;            // channel A's current gain per bin (dB), for the display

private:
    void frame(int last, const float* p, const bool* active) {
        const float binNorm = 2.f / winLin;              // a sine at -X dBFS reads -X dB in its bin
        bool needSc = false;
        for (int b = 0; b < kNumBands; ++b) if (active[b] && p[bp2(b, kB2Source)] > 0.5f) needSc = true;
        for (int c = 0; c < 2; ++c) {
            analyse(in[c], last, X[c]);
            if (needSc) analyse(sc[c], last, S[c]);
        }
        std::fill(gc[0].begin(), gc[0].end(), 0.f); std::fill(gc[1].begin(), gc[1].end(), 0.f);
        const float hopSec = (float)H / sr;
        for (int b = 0; b < kNumBands; ++b) {
            if (!active[b]) { std::fill(g[b].begin(), g[b].end(), 0.f); continue; }
            weights(b, p);
            const int place = (int)(p[bp(b, kBPlace)] + 0.5f);
            const bool ext = p[bp2(b, kB2Source)] > 0.5f;
            const float thr = p[bp2(b, kB2Thresh)], range = p[bp2(b, kB2Range)] * gainScaleOf(p);
            const float aA = 1.f - std::exp(-hopSec / (0.001f * std::max(0.1f, p[bp2(b, kB2Attack)])));
            const float aR = 1.f - std::exp(-hopSec / (0.001f * std::max(1.f, p[bp2(b, kB2Release)])));
            std::vector<float>& gb = g[b]; const std::vector<float>& wb = w[b];
            for (int k = 1; k < K; ++k) {
                if (wb[k] <= 1e-3f) { gb[k] += (0.f - gb[k]) * aR; continue; }
                float mag = 0;
                for (int c = 0; c < 2; ++c) if (placeApplies(place, c)) mag = std::max(mag, std::abs(ext ? S[c][k] : X[c][k]));
                const float lvl = 20.f * std::log10(mag * binNorm + 1e-9f);
                const float target = wb[k] * dynChange(lvl, thr, range);
                const float a = std::fabs(target) > std::fabs(gb[k]) ? aA : aR;
                gb[k] += (target - gb[k]) * a;
            }
            for (int c = 0; c < 2; ++c) if (placeApplies(place, c)) for (int k = 0; k < K; ++k) gc[c][k] += gb[k];
        }
        for (int k = 0; k < K; ++k) show[k] = gc[0][k];
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < K; ++k) {
                const float gl = gc[c][k] == 0.f ? 1.f : std::pow(10.f, gc[c][k] / 20.f);
                buf[k] = X[c][k] * gl; if (k > 0 && k < N / 2) buf[N - k] = std::conj(buf[k]);
            }
            fft.run(buf.data(), true);
            // overlap-add: sqrt-Hann analysis and synthesis at 75% overlap sum to 2
            for (int i = 0; i < N; ++i) out[c][(last + 1 + i) % N] += buf[i].real() * win[i] * 0.5f;
        }
    }
    void analyse(const std::vector<float>& ring, int last, std::vector<cpx>& dst) {
        for (int i = 0; i < N; ++i) buf[i] = cpx(ring[(last + 1 + i) % N] * win[i], 0.f);
        fft.run(buf.data(), false);
        for (int k = 0; k < K; ++k) dst[k] = buf[k];
    }
    // how much of each bin belongs to band b: its normalised dB curve at +12 dB
    void weights(int b, const float* p) {
        const float key = p[bp(b, kBShape)] * 1e6f + p[bp(b, kBFreq)] * 7.f + p[bp(b, kBQ)] * 131.f + (p[bp(b, kBGain)] >= 0 ? 1.f : 0.f) + p[slopeParam(p, b)] * 3.f;
        if (key == wKey[b]) return;
        wKey[b] = key;
        const eq::Spec s = bandSpec(p, b, 12.f);
        eq::Design d; eq::design(s, sr, d);
        for (int k = 0; k < K; ++k) {
            const float hz = std::max(1.f, binHz(k));
            const float db = 20.f * std::log10(std::max(1e-9f, std::abs(eq::response(d, hz))));
            float v = s.shape == kTiltShelf ? (db + 6.f) / 12.f : (s.shape == kFlatTilt ? 1.f : db / 12.f);
            w[b][k] = clampf(v, 0.f, 1.f);
        }
    }

    int N = 1024, H = 256, K = 513; float sr = 48000.f; uint32_t t = 0; float winLin = 1;
    Fft fft;
    std::vector<float> win, in[2], sc[2], out[2];
    std::vector<cpx> buf, X[2], S[2];
    std::vector<float> g[kNumBands], w[kNumBands], gc[2];
    float wKey[kNumBands];
};

// ------------------------------------------------------------------ engine
class Engine {
public:
    float p[kParamCount];

    Engine() {
        std::fill(p, p + kParamCount, 0.f);
        p[kProcMode] = kZeroLatency; p[kLinQuality] = 1; p[kChannelMode] = 0; p[kOutGain] = 0; p[kAnalyzer] = 2; p[kRange] = 1; p[kMidiOn] = 1;
        p[kCharacter] = kClean; p[kAutoGain] = 0; p[kGainScale] = 100; p[kPhaseInvert] = 0;
        p[kAnaExternal] = 0; p[kAnaRange] = 1; p[kAnaSpeed] = 2; p[kAnaRes] = 1; p[kAnaTilt] = 4.5f; p[kAnaFreeze] = 0; p[kCollision] = 0; p[kPiano] = 0;
        for (int b = 0; b < kNumBands; ++b) defaultBand(b, 1000.f);
        for (int i = 0; i < kAnaSize; ++i) anaPre[i] = anaPost[i] = anaSc[i] = 0.f;
    }
    void defaultBand(int b, float hz) {
        p[bp(b, kBUsed)] = 0; p[bp(b, kBOn)] = 1; p[bp(b, kBShape)] = kBell; p[bp(b, kBFreq)] = hz;
        p[bp(b, kBGain)] = 0; p[bp(b, kBQ)] = 1; p[bp(b, kBSlope)] = 1; p[bp(b, kBPlace)] = kPlaceBoth;
        p[bp2(b, kB2Slope)] = 1; p[bp2(b, kB2Dyn)] = 0; p[bp2(b, kB2Range)] = 0; p[bp2(b, kB2Thresh)] = -30; p[bp2(b, kB2Attack)] = 10;
        p[bp2(b, kB2Release)] = 100; p[bp2(b, kB2Source)] = 0; p[bp2(b, kB2ScFree)] = 0; p[bp2(b, kB2ScFreq)] = hz; p[bp2(b, kB2ScQ)] = 1; p[bp2(b, kB2Spectral)] = 0;
    }
    void setSampleRate(float sr) {
        fs = sr;
        smooth = 1.f - std::exp(-1.f / (0.015f * sr * 1.f / kCtl));
        fadeStep = (float)kCtl / (0.008f * sr);
        holdTicks = (int)(0.03f * sr / kCtl);
        const int m = sr <= 50000.f ? 1 : (sr <= 100000.f ? 2 : 4);
        natK = 512 * m; natD = natK / 4; natB = 64 * m;
        for (int c = 0; c < 2; ++c) { natConv[c].init(natB, natK); natIn[c].assign(natB, 0.f); natOut[c].assign(natB, 0.f); }
        natFft.init(natK); natBuf.assign(natK, cpx(0, 0)); natSpec[0].assign(natK / 2 + 1, cpx(0, 0)); natSpec[1].assign(natK / 2 + 1, cpx(0, 0)); natH.assign(natK, 0.f);
        spectral.init(sr);
        reset();
    }
    void reset() {
        mode = clampi((int)(p[kProcMode] + 0.5f), 0, kNumProcModes - 1); linQ = clampi((int)(p[kLinQuality] + 0.5f), 0, 2);
        for (int i = 0; i < kNumBands; ++i) {
            Band& b = bands[i];
            b.reset(); syncTargets(b, i); b.curShape = b.shape; b.curSlope = b.slope; b.curPlace = b.place;
            b.oct = std::log2(clampf(p[bp(i, kBFreq)], 10.f, 30000.f)); b.gain = p[bp(i, kBGain)] * gainScaleOf(p);
            b.lq = std::log2(clampf(p[bp(i, kBQ)], 0.025f, 40.f)); b.mix = b.m = (b.used && b.on) ? 1.f : 0.f; b.dm = 0; b.dynDb = 0;
            designBand(b, i);
        }
        outTarget = p[kOutGain]; autoDb = autoGainDb(); sOut = dbToGain(p[kOutGain] + (p[kAutoGain] > 0.5f ? autoDb : 0.f)); ctl = 0;
        setupLinear(true);
        natPos = 0; for (int c = 0; c < 2; ++c) { natConv[c].reset(); std::fill(natIn[c].begin(), natIn[c].end(), 0.f); std::fill(natOut[c].begin(), natOut[c].end(), 0.f); }
        natSnap.clear(); natSnap.reserve(kParamCount + 1); natTmp.reserve(kParamCount + 1); natCountdown = 0; if (mode == kNaturalPhase) natUpdate(true);
        for (int c = 0; c < 2; ++c) { dry[c].assign(kDrySize, 0.f); scDel[c].assign(kDrySize, 0.f); dcW[c] = 0; }
        dryPos = 0;
        spectral.reset(); specOn = anySpectral();
        solo.reset();
        agCountdown = 0;
    }

    // latency (samples) the host must compensate
    uint32_t eqLatency() const {
        if (mode == kLinearPhase) return (uint32_t)(firN + firN / 2);
        if (mode == kNaturalPhase) return (uint32_t)(natB + natD);
        return 0;
    }
    uint32_t latency() const { return eqLatency() + (specOn ? (uint32_t)spectral.latency() : 0u); }
    // what latency() will be once the current parameters take effect (the plugin reports it before processing)
    uint32_t wantedLatency() const {
        const int md = clampi((int)(p[kProcMode] + 0.5f), 0, kNumProcModes - 1);
        uint32_t l = md == kLinearPhase ? (uint32_t)((2048 << clampi((int)(p[kLinQuality] + 0.5f), 0, 2)) * 3 / 2) : (md == kNaturalPhase ? (uint32_t)(natB + natD) : 0u);
        if (anySpectral()) l += (uint32_t)spectral.latency();
        return l;
    }
    bool anySpectral() const { for (int b = 0; b < kNumBands; ++b) if (bandActive(p, b) && bandSpectral(p, b)) return true; return false; }

    // analyser feeds: rings of mono samples (input, output, sidechain), read by the UI
    static constexpr int kAnaSize = 16384;
    float anaPre[kAnaSize], anaPost[kAnaSize], anaSc[kAnaSize];
    AtomicU32 anaWrite;
    AtomicF peakOut[2], peakIn[2];
    AtomicF dynShow[kNumBands];                          // current dynamic gain change per band (dB)
    int soloBand = -1;                                   // set by the plugin from the UI
    float sampleRate() const { return fs; }
    const std::vector<float>& spectralGain() const { return spectral.show; }
    float spectralBinHz(int k) const { return (float)k * fs / (float)spectral.latency(); }
    float autoGain() const { return autoDb; }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) { process(inL, inR, nullptr, nullptr, outL, outR, n); }
    void process(const float* inL, const float* inR, const float* scL, const float* scR, float* outL, float* outR, uint32_t n) {
        const int wantMode = clampi((int)(p[kProcMode] + 0.5f), 0, kNumProcModes - 1);
        const int q = clampi((int)(p[kLinQuality] + 0.5f), 0, 2);
        if (wantMode != mode || (mode == kLinearPhase && q != linQ)) {
            mode = wantMode; linQ = q; setupLinear(true);
            for (int i = 0; i < kNumBands; ++i) { bands[i].f.reset(); designBand(bands[i], i); }
            natPos = 0; for (int c = 0; c < 2; ++c) { natConv[c].reset(); std::fill(natIn[c].begin(), natIn[c].end(), 0.f); std::fill(natOut[c].begin(), natOut[c].end(), 0.f); }
            natSnap.clear(); if (mode == kNaturalPhase) natUpdate(true);
        }
        const bool wantSpec = anySpectral();
        if (wantSpec != specOn) { specOn = wantSpec; spectral.reset(); }
        const bool ms = p[kChannelMode] > 0.5f;
        const int chr = clampi((int)(p[kCharacter] + 0.5f), 0, kNumCharacters - 1);
        const float inv = p[kPhaseInvert] > 0.5f ? -1.f : 1.f;
        const uint32_t lat = eqLatency();
        bool specActive[kNumBands]; for (int b = 0; b < kNumBands; ++b) specActive[b] = bandActive(p, b) && bandSpectral(p, b);
        uint32_t w = anaWrite.load(std::memory_order_relaxed);
        float pkI[2] = { 0, 0 }, pkO[2] = { 0, 0 };

        for (uint32_t i = 0; i < n; ++i) {
            if (--ctl <= 0) { ctl = kCtl; controlUpdate(); }
            float a = inL[i], b = inR[i];
            float sa = scL ? scL[i] : 0.f, sb = scR ? scR[i] : 0.f;
            pkI[0] = std::max(pkI[0], std::fabs(a)); pkI[1] = std::max(pkI[1], std::fabs(b));
            const uint32_t wi = w & (kAnaSize - 1);
            anaPre[wi] = 0.5f * (a + b); anaSc[wi] = 0.5f * (sa + sb);
            if (ms) { const float m = 0.5f * (a + b), s = 0.5f * (a - b); a = m; b = s; const float m2 = 0.5f * (sa + sb), s2 = 0.5f * (sa - sb); sa = m2; sb = s2; }
            const float x[2] = { a, b }, side[2] = { sa, sb };

            // dynamic detection (broadband bands; spectral bands detect inside the spectral stage)
            for (Band& bd : bands) {
                if (!bd.used || !bd.dyn || bd.spectral) continue;
                float lvl = 0;
                for (int c = 0; c < 2; ++c) {
                    if (!bd.applies(c)) continue;
                    const float src = bd.external ? side[c] : x[c];
                    lvl = std::max(lvl, std::fabs(bd.detAll ? src : bd.det.process(src, c)));
                }
                bd.env += (lvl - bd.env) * (lvl > bd.env ? bd.aA : bd.aR);
            }

            // the equalizer
            float y[2] = { a, b };
            if (mode == kLinearPhase) {
                linIn[0][linPos] = a; linIn[1][linPos] = b;
                y[0] = linOut[0][linPos]; y[1] = linOut[1][linPos];
                if (++linPos == firN) { linPos = 0; linearBlock(); }
            }
            for (Band& bd : bands) {
                if (!bd.used || !bd.run) continue;           // used bands always run, so switching them on is click-free
                bd.m += bd.dm;
                for (int c = 0; c < 2; ++c) {
                    if (!bd.applies(c)) continue;
                    const float f = bd.f.process(y[c], c);
                    if (bd.m > 0.f) y[c] += (f - y[c]) * bd.m;
                }
            }
            if (mode == kNaturalPhase) {
                natIn[0][natPos] = y[0]; natIn[1][natPos] = y[1];
                y[0] = natOut[0][natPos]; y[1] = natOut[1][natPos];
                if (++natPos == natB) {
                    natPos = 0;
                    if (--natCountdown <= 0) { natCountdown = std::max(1, 1024 / natB); natUpdate(false); }
                    for (int c = 0; c < 2; ++c) natConv[c].process(natIn[c].data(), natOut[c].data());
                }
            }

            // the dry signal, aligned with the equalizer's output
            dry[0][dryPos] = a; dry[1][dryPos] = b; scDel[0][dryPos] = sa; scDel[1][dryPos] = sb;
            const uint32_t rd = (dryPos + kDrySize - lat) & (kDrySize - 1);
            const float da[2] = { dry[0][rd], dry[1][rd] }, sd[2] = { scDel[0][rd], scDel[1][rd] };
            dryPos = (dryPos + 1) & (kDrySize - 1);

            if (chr != kClean) for (int c = 0; c < 2; ++c) y[c] = da[c] + character(y[c] - da[c], c, chr);
            if (soloBand >= 0 && soloBand < kNumBands) { y[0] = solo.process(da[0], 0); y[1] = solo.process(da[1], 1); }
            if (specOn) spectral.process(y, sd, p, specActive);

            if (ms) { const float m = y[0], s = y[1]; y[0] = m + s; y[1] = m - s; }
            sOut += (outTargetLin - sOut) * 0.002f;
            y[0] *= sOut * inv; y[1] *= sOut * inv;
            if (!(std::fabs(y[0]) < 1e6f)) y[0] = 0;
            if (!(std::fabs(y[1]) < 1e6f)) y[1] = 0;
            anaPost[wi] = 0.5f * (y[0] + y[1]); ++w;
            pkO[0] = std::max(pkO[0], std::fabs(y[0])); pkO[1] = std::max(pkO[1], std::fabs(y[1]));
            outL[i] = y[0]; outR[i] = y[1];
        }
        anaWrite.store(w, std::memory_order_release);
        for (int c = 0; c < 2; ++c) {
            if (pkO[c] > peakOut[c].load()) peakOut[c].store(pkO[c]);
            if (pkI[c] > peakIn[c].load()) peakIn[c].store(pkI[c]);
        }
        for (int i = 0; i < kNumBands; ++i) {
            Band& bd = bands[i];
            if (!bd.f.finite() || !bd.det.finite()) { bd.reset(); }
            dynShow[i].store(bd.dyn && !bd.spectral ? bd.dynDb : 0.f);
        }
    }

    // combined static response at one frequency for channel A (0) or B (1) (gain scale applied, dynamics not)
    cpx response(float hz, int c) const { return responseWith(hz, c, nullptr); }
    // the same with the current dynamic gain changes, for the display
    cpx responseDynamic(float hz, int c) const {
        float d[kNumBands]; for (int b = 0; b < kNumBands; ++b) d[b] = dynShow[b].load();
        return responseWith(hz, c, d);
    }
    cpx bandResponse(int b, float hz, float extraDb = 0.f) const {
        eq::Design d; eq::design(bandSpec(p, b, p[bp(b, kBGain)] * gainScaleOf(p) + extraDb), fs, d);
        return eq::response(d, hz);
    }

private:
    static constexpr int kCtl = 16;
    static constexpr uint32_t kDrySize = 16384;

    cpx responseWith(float hz, int c, const float* dynDb) const {
        cpx h(1.f, 0.f);
        const float gs = gainScaleOf(p);
        for (int b = 0; b < kNumBands; ++b) {
            if (!bandActive(p, b)) continue;
            if (!placeApplies((int)(p[bp(b, kBPlace)] + 0.5f), c)) continue;
            eq::Design d; eq::design(bandSpec(p, b, p[bp(b, kBGain)] * gs + (dynDb ? dynDb[b] : 0.f)), fs, d);
            h *= eq::response(d, hz);
        }
        return h;
    }

    void syncTargets(Band& b, int i) {
        b.used = p[bp(i, kBUsed)] > 0.5f; b.on = p[bp(i, kBOn)] > 0.5f;
        b.shape = clampi((int)(p[bp(i, kBShape)] + 0.5f), 0, kNumShapes - 1); b.slope = bandSlopeDb(p, i); b.place = (int)(p[bp(i, kBPlace)] + 0.5f);
        b.dyn = bandDynamic(p, i); b.spectral = b.dyn && p[bp2(i, kB2Spectral)] > 0.5f; b.external = p[bp2(i, kB2Source)] > 0.5f;
    }
    // static gain (scaled) plus the dynamic change; in linear phase only the dynamic change runs as a filter
    void designBand(Band& b, int i) {
        b.run = mode != kLinearPhase || (b.dyn && !b.spectral);
        eq::Spec s; s.shape = b.curShape; s.freq = std::exp2(b.oct); s.q = std::exp2(b.lq); s.slopeDb = b.curSlope;
        s.gainDb = mode == kLinearPhase ? b.dynDb : b.gain + b.dynDb;
        const float key[6] = { (float)s.shape, s.freq, s.q, (float)s.slopeDb, s.gainDb, (float)mode };
        if (std::memcmp(key, b.key, sizeof key) != 0) {          // settled bands keep their coefficients
            std::memcpy(b.key, key, sizeof key);
            eq::Design d; eq::design(s, fs, d);
            b.f.set(d, fs, s.shape == kFlatTilt ? 0.f : s.freq);
        }
        // detection filter: the band's region, or a free band-pass
        b.detAll = false;
        if (b.dyn && !b.spectral) {
            eq::Spec ds; ds.q = 0.70710678f; ds.slopeDb = 12;
            if (p[bp2(i, kB2ScFree)] > 0.5f) { ds.shape = kBandPass; ds.slopeDb = 6; ds.freq = clampf(p[bp2(i, kB2ScFreq)], 10.f, 30000.f); ds.q = clampf(p[bp2(i, kB2ScQ)], 0.1f, 40.f); }
            else if (b.curShape == kBell) { ds.shape = kBandPass; ds.slopeDb = 6; ds.freq = s.freq; ds.q = std::max(0.3f, s.q); }
            else if (b.curShape == kLowShelf) { ds.shape = kHighCut; ds.freq = s.freq; }
            else if (b.curShape == kHighShelf) { ds.shape = kLowCut; ds.freq = s.freq; }
            else b.detAll = true;
            if (!b.detAll) { eq::Design dd; eq::design(ds, fs, dd); b.det.set(dd, fs, ds.freq); }
            b.aA = 1.f - std::exp(-1.f / (0.001f * std::max(0.1f, p[bp2(i, kB2Attack)]) * fs));
            b.aR = 1.f - std::exp(-1.f / (0.001f * std::max(1.f, p[bp2(i, kB2Release)]) * fs));
        }
    }

    // every kCtl samples: smooth controls, handle structural changes with a short fade, rebuild coefficients
    void controlUpdate() {
        const float gs = gainScaleOf(p);
        for (int i = 0; i < kNumBands; ++i) {
            Band& b = bands[i];
            syncTargets(b, i);
            const bool structural = b.shape != b.curShape || b.slope != b.curSlope || b.place != b.curPlace;
            if (structural && (b.mix > 0.f || b.m > 0.f)) b.swapping = true;
            if (structural && b.mix <= 0.f && b.m <= 0.f) { b.curShape = b.shape; b.curSlope = b.slope; b.curPlace = b.place; b.reset(); b.swapping = false; b.hold = holdTicks; }
            if (b.hold > 0) --b.hold;
            const float target = (b.used && b.on && !b.swapping && b.hold == 0) ? 1.f : 0.f;
            if (b.mix < target) b.mix = std::min(target, b.mix + fadeStep);
            else if (b.mix > target) b.mix = std::max(target, b.mix - fadeStep);
            if (b.mix <= 0.f && b.m <= 0.f && b.swapping) { b.curShape = b.shape; b.curSlope = b.slope; b.curPlace = b.place; b.swapping = false; b.reset(); b.hold = holdTicks; }
            b.m = clampf(b.m, 0.f, 1.f); if (std::fabs(b.m - b.mix) < 1e-4f) b.m = b.mix;
            b.dm = (b.mix - b.m) / kCtl;
            if (!b.used) { b.reset(); b.m = b.mix = 0; b.dm = 0; b.dynDb = 0; continue; }
            const float tOct = std::log2(clampf(p[bp(i, kBFreq)], 10.f, 30000.f)), tG = p[bp(i, kBGain)] * gs, tQ = std::log2(clampf(p[bp(i, kBQ)], 0.025f, 40.f));
            if (b.mix <= 0.f && b.hold > 0) { b.oct = tOct; b.gain = tG; b.lq = tQ; }
            else { b.oct += (tOct - b.oct) * smooth; b.gain += (tG - b.gain) * smooth; b.lq += (tQ - b.lq) * smooth; }
            if (b.dyn && !b.spectral && b.on) b.dynDb = dynChange(20.f * std::log10(b.env + 1e-9f), p[bp2(i, kB2Thresh)], p[bp2(i, kB2Range)] * gs);
            else b.dynDb += (0.f - b.dynDb) * 0.2f;
            designBand(b, i);
        }
        // output gain and auto gain
        if (--agCountdown <= 0) { agCountdown = 2048 / kCtl; if (p[kAutoGain] > 0.5f) autoDb = autoGainDb(); }
        outTargetLin = dbToGain(p[kOutGain] + (p[kAutoGain] > 0.5f ? autoDb : 0.f));
        // solo filter
        if (soloBand >= 0 && soloBand < kNumBands) {
            const int s = soloBand; const int shape = (int)(p[bp(s, kBShape)] + 0.5f);
            eq::Spec ds; ds.freq = clampf(p[bp(s, kBFreq)], 10.f, 30000.f); ds.q = 0.70710678f; ds.slopeDb = 12; ds.gainDb = 0;
            if (shape == kLowShelf || shape == kLowCut) ds.shape = kHighCut;          // hear what the band changes or removes
            else if (shape == kHighShelf || shape == kHighCut) ds.shape = kLowCut;
            else if (shape == kTiltShelf || shape == kFlatTilt) { ds.shape = kAllPass; }
            else { ds.shape = kBandPass; ds.slopeDb = 6; ds.q = std::max(0.5f, clampf(p[bp(s, kBQ)], 0.025f, 40.f)); }
            if (isCutShape(shape)) ds.slopeDb = std::max(12, bandSlopeDb(p, s) == eq::kBrickwall ? 96 : bandSlopeDb(p, s));
            eq::Design d; eq::design(ds, fs, d); solo.set(d, fs, ds.freq);
        }
    }

    // auto gain: keeps the loudness of pink noise about the same as the curve changes
    float autoGainDb() const {
        double s = 0; int n = 0;
        for (float f = 20.f; f <= 20000.f; f *= 1.15f) { for (int c = 0; c < 2; ++c) { const float m = std::abs(response(f, c)); s += (double)m * m; } n += 2; }
        return clampf(-10.f * (float)std::log10(std::max(1e-12, s / std::max(1, n))), -24.f, 24.f);
    }

    // Gentle and Warm add saturation to what the EQ changes; a flat EQ stays clean
    float character(float d, int c, int chr) {
        if (chr == kGentle) return std::tanh(0.7f * d) / 0.7f;
        const float s = std::tanh(1.4f * d) / 1.4f, e = 0.12f * d * d;
        dcW[c] += (e - dcW[c]) * dcCoef();
        return s + e - dcW[c];
    }
    float dcCoef() const { return 1.f - std::exp(-2.f * kPi * 10.f / fs); }

    // ---------------------------------------------------------------- natural phase
    // correction FIR: analog response / digital response of the static settings, pre-delayed by natD
    void natUpdate(bool force) {
        natTmp.assign(p + kBandFirst, p + kParamCount);          // no allocation once the capacity is there
        natTmp.push_back(p[kChannelMode]);
        if (!force && natTmp == natSnap) return;
        natSnap.assign(natTmp.begin(), natTmp.end());
        const float gs = gainScaleOf(p);
        eq::Design ds[kNumBands]; bool act[kNumBands]; int pl[kNumBands];
        for (int b = 0; b < kNumBands; ++b) {
            act[b] = bandActive(p, b); pl[b] = (int)(p[bp(b, kBPlace)] + 0.5f);
            if (act[b]) eq::design(bandSpec(p, b, p[bp(b, kBGain)] * gs), fs, ds[b]);
        }
        const int K = natK;
        // per-section pre-warp factors, computed once (the digital response needs one tan per section)
        int nsec = 0;
        for (int bb = 0; bb < kNumBands; ++bb) if (act[bb]) {
            const int shape = (int)(p[bp(bb, kBShape)] + 0.5f);
            const float warp = shape == kFlatTilt ? 0.f : clampf(p[bp(bb, kBFreq)], 10.f, 30000.f);
            for (int j = 0; j < ds[bb].n && nsec < kMaxNatSec; ++j) { natG[nsec] = eq::Cascade<2>::warpG(ds[bb].s[j], fs, warp); natSecBand[nsec] = bb; natSec[nsec] = ds[bb].s[j]; ++nsec; }
        }
        for (int k = 0; k <= K / 2; ++k) {
            const float hz = std::max(1.f, (float)k * fs / K);
            const float w = 2.f * kPi * hz / fs;
            const cpx z1 = std::polar(1.f, -w), bil = (1.f - z1) / (1.f + z1);
            cpx an[2] = { cpx(1, 0), cpx(1, 0) }, dg[2] = { cpx(1, 0), cpx(1, 0) };
            for (int j = 0; j < nsec; ++j) {
                const eq::Section& x = natSec[j];
                const cpx ha = eq::sectionResponse(x, hz);
                const cpx sn = bil / natG[j];
                const cpx hd = x.order == 1 ? (x.b1 * sn + x.b0) / (sn + 1.f) : (x.b2 * sn * sn + x.b1 * sn + x.b0) / (sn * sn + x.k * sn + 1.f);
                for (int c = 0; c < 2; ++c) if (placeApplies(pl[natSecBand[j]], c)) { an[c] *= ha; dg[c] *= hd; }
            }
            for (int c = 0; c < 2; ++c) {
                cpx corr = std::abs(dg[c]) < 1e-5f ? cpx(1.f, 0.f) : an[c] / dg[c];
                const float mag = std::abs(corr);
                if (mag > 16.f) corr *= 16.f / mag;
                natSpec[c][k] = corr * std::polar(1.f, -2.f * kPi * (float)k * natD / K);     // pre-delay
            }
        }
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k <= K / 2; ++k) { natBuf[k] = natSpec[c][k]; if (k > 0 && k < K / 2) natBuf[K - k] = std::conj(natSpec[c][k]); }
            natBuf[K / 2] = cpx(natBuf[K / 2].real(), 0.f);
            natFft.run(natBuf.data(), true);
            for (int i = 0; i < K; ++i) {
                float wdw = 1.f;
                if (i < natD) wdw = 0.5f - 0.5f * std::cos(kPi * i / natD);
                else if (i > K * 3 / 4) wdw = 0.5f + 0.5f * std::cos(kPi * (i - K * 3 / 4) / (K / 4));
                natH[i] = natBuf[i].real() * wdw;
            }
            natConv[c].setKernel(natH.data(), K);
        }
    }

    // ---------------------------------------------------------------- linear phase
    void setupLinear(bool clear) {
        firN = 2048 << clampi(linQ, 0, 2);
        fft.init(2 * firN); ifft.init(firN);
        for (int c = 0; c < 2; ++c) {
            linIn[c].assign(firN, 0.f); linOut[c].assign(firN, 0.f); overlap[c].assign(firN, 0.f);
            kernel[c].assign(2 * firN, cpx(0, 0)); kernelOld[c].assign(2 * firN, cpx(0, 0));
        }
        work.assign(2 * firN, cpx(0, 0)); work2.assign(2 * firN, cpx(0, 0)); design.assign(firN, cpx(0, 0));
        linPos = 0; designed.assign(kParamCount, -12345.f);
        if (clear && mode == kLinearPhase) { for (int c = 0; c < 2; ++c) buildKernel(c, kernel[c]); snapshotParams(); }
    }
    void snapshotParams() { for (uint32_t i = kChannelMode; i < kParamCount; ++i) designed[i] = p[i]; }
    bool paramsChanged() const {
        for (uint32_t i = kBandFirst; i <= kBandLast; ++i) if (designed[i] != p[i]) return true;
        if (designed[kGainScale] != p[kGainScale]) return true;
        for (int b = 0; b < kNumBands; ++b) if (designed[bp2(b, kB2Slope)] != p[bp2(b, kB2Slope)]) return true;
        return false;
    }
    // FIR (length firN, linear phase) from the analog magnitude response, as a 2N-point spectrum for overlap-add
    void buildKernel(int c, std::vector<cpx>& K) {
        const int N = firN;
        const float gs = gainScaleOf(p);
        int nd = 0;
        for (int b = 0; b < kNumBands; ++b)
            if (bandActive(p, b) && placeApplies((int)(p[bp(b, kBPlace)] + 0.5f), c)) eq::design(bandSpec(p, b, p[bp(b, kBGain)] * gs), fs, kDesigns[nd++]);
        for (int k = 0; k <= N / 2; ++k) {
            const float hz = std::max(1.f, (float)k * fs / N);
            cpx h(1.f, 0.f); for (int j = 0; j < nd; ++j) h *= eq::response(kDesigns[j], hz);
            const float mag = std::min(std::abs(h), 1000.f);
            design[k] = cpx(mag, 0.f); if (k > 0 && k < N / 2) design[N - k] = cpx(mag, 0.f);
        }
        ifft.run(design.data(), true);
        for (int i = 0; i < 2 * N; ++i) work2[i] = cpx(0, 0);
        for (int i = 0; i < N; ++i) {
            const float wdw = 0.42f - 0.5f * std::cos(2 * kPi * i / (N - 1)) + 0.08f * std::cos(4 * kPi * i / (N - 1));   // Blackman
            work2[i] = cpx(design[(i - N / 2 + N) % N].real() * wdw, 0.f);
        }
        fft.run(work2.data(), false);
        K = work2;
    }
    void linearBlock() {
        const int N = firN;
        const bool changed = paramsChanged();
        if (changed) { for (int c = 0; c < 2; ++c) { kernelOld[c] = kernel[c]; buildKernel(c, kernel[c]); } snapshotParams(); }
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < N; ++i) work[i] = cpx(linIn[c][i], 0.f);
            for (int i = N; i < 2 * N; ++i) work[i] = cpx(0, 0);
            fft.run(work.data(), false);
            if (changed) {   // crossfade from the old to the new kernel across this block
                for (int i = 0; i < 2 * N; ++i) work2[i] = work[i] * kernelOld[c][i];
                fft.run(work2.data(), true);
            }
            for (int i = 0; i < 2 * N; ++i) work[i] *= kernel[c][i];
            fft.run(work.data(), true);
            for (int i = 0; i < N; ++i) {
                float v = work[i].real();
                if (changed) { const float t = (float)i / N; v = work2[i].real() * (1.f - t) + v * t; }
                linOut[c][i] = v + overlap[c][i];
                overlap[c][i] = work[i + N].real();
            }
        }
    }

    Band bands[kNumBands];
    eq::Design kDesigns[kNumBands];
    eq::Cascade<2> solo;
    Spectral spectral; bool specOn = false;
    float fs = 48000.f, smooth = 0.1f, fadeStep = 0.05f, sOut = 1.f, outTarget = 0.f, outTargetLin = 1.f, autoDb = 0.f; int holdTicks = 90;
    int ctl = 0, agCountdown = 0, mode = kZeroLatency;
    // linear phase
    int linQ = 1, firN = 4096, linPos = 0;
    Fft fft, ifft;
    std::vector<float> linIn[2], linOut[2], overlap[2], designed;
    std::vector<cpx> kernel[2], kernelOld[2], work, work2, design;
    // natural phase
    int natK = 512, natD = 128, natB = 64, natPos = 0, natCountdown = 0;
    static constexpr int kMaxNatSec = kNumBands * eq::kMaxSections;
    float natG[kMaxNatSec]; int natSecBand[kMaxNatSec]; eq::Section natSec[kMaxNatSec]; std::vector<cpx> natSpec[2];
    Convolver natConv[2]; Fft natFft; std::vector<cpx> natBuf; std::vector<float> natH, natIn[2], natOut[2], natSnap, natTmp;
    // dry / sidechain alignment, character DC trackers
    std::vector<float> dry[2], scDel[2]; uint32_t dryPos = 0; float dcW[2] = { 0, 0 };
};

} // namespace mica
