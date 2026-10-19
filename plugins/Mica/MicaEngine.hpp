// Mica — 24-band stereo equalizer engine: zero-latency (TPT state-variable filters) or linear-phase
// (FIR designed from the analog prototypes, FFT overlap-add), stereo or mid/side, per-band channel placement.
// Clean-room implementation from public, generic techniques (Zavalishin/Simper trapezoidal SVF, RBJ-style
// analog prototypes, frequency-sampling FIR design with windowing). No third-party product code, assets or
// disassembly were used. License: MIT (see LICENSE).

#pragma once
#include "Fft.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mica {

static constexpr float kPi = 3.14159265358979f;
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }

static constexpr int kNumBands = 24;
enum BandField { kBUsed, kBOn, kBShape, kBFreq, kBGain, kBQ, kBSlope, kBPlace, kBandFields };

// ------------------------------------------------------------------ params (append-only)
enum Param : uint32_t {
    kProcMode, kLinQuality, kChannelMode, kOutGain, kAnalyzer, kRange, kMidiOn,
    kBandFirst,
    kBandLast = kBandFirst + kNumBands * kBandFields - 1,
    kOutLearn,
    kParamCount
};
inline uint32_t bp(int band, int field) { return kBandFirst + band * kBandFields + field; }
enum Shape { kBell, kLowShelf, kHighShelf, kLowCut, kHighCut, kNumShapes };
enum Place { kPlaceBoth, kPlaceA, kPlaceB };              // A = left or mid, B = right or side
inline int slopeDb(int s) { static const int t[4] = { 6, 12, 24, 48 }; return t[s < 0 ? 0 : (s > 3 ? 3 : s)]; }
inline int cutStages(int s) { return s <= 1 ? 1 : (s == 2 ? 2 : 4); }   // slope 0 uses a one-pole stage
inline const float* bwK(int stages) {
    static const float k1[1] = { 1.41421356f };
    static const float k2[2] = { 1.84775907f, 0.76536686f };
    static const float k4[4] = { 1.96157056f, 1.66293922f, 1.11114047f, 0.39018064f };
    return stages == 1 ? k1 : (stages == 2 ? k2 : k4);
}

// ------------------------------------------------------------------ analog prototype response of one band
// (used for the display, for the linear-phase FIR, and matched by the zero-latency filters away from Nyquist)
inline cpx bandResponse(int shape, int slope, float fc, float gainDb, float q, float hz) {
    const cpx s(0.f, hz / fc);
    const float A = std::pow(10.f, gainDb / 40.f), sq = std::sqrt(A);
    switch (shape) {
    case kBell:      return (s * s + s * (A / q) + 1.f) / (s * s + s / (A * q) + 1.f);
    case kLowShelf:  return A * (s * s + s * (sq / q) + A) / (A * s * s + s * (sq / q) + 1.f);
    case kHighShelf: return A * (A * s * s + s * (sq / q) + 1.f) / (s * s + s * (sq / q) + A);
    default: {
        const bool hp = shape == kLowCut;
        if (slope == 0) return hp ? s / (s + 1.f) : cpx(1.f, 0.f) / (s + 1.f);
        const int st = cutStages(slope); const float* kb = bwK(st);
        cpx h(1.f, 0.f);
        for (int i = 0; i < st; ++i) {
            const float k = i == st - 1 ? kb[i] * 0.70710678f / q : kb[i];
            h *= (hp ? s * s : cpx(1.f, 0.f)) / (s * s + k * s + 1.f);
        }
        return h;
    }
    }
}

// ------------------------------------------------------------------ zero-latency band
struct Band {
    // targets (from parameters)
    bool used = false, on = false; int shape = kBell, slope = 1, place = kPlaceBoth;
    // what the filters are currently built as (changes go through a short fade out / in)
    int curShape = kBell, curSlope = 1, curPlace = kPlaceBoth; bool swapping = false; int hold = 0;   // hold: control ticks to let a rebuilt filter settle before fading in
    // smoothed controls
    float oct = 10, gain = 0, lq = 0, mix = 0, m = 0, dm = 0;   // mix: per control tick; m/dm: ramped per sample
    // coefficients
    int nst = 1; bool onePole = false;
    float a1[4] = {}, a2[4] = {}, a3[4] = {}, m0[4] = {}, m1[4] = {}, m2[4] = {}, G = 0;
    float ic1[2][4] = {}, ic2[2][4] = {}, op[2] = {};
    void reset() { std::memset(ic1, 0, sizeof ic1); std::memset(ic2, 0, sizeof ic2); op[0] = op[1] = 0; }

    void design(float fs) {
        const float fc = clampf(std::exp2(oct), 5.f, 0.49f * fs), q = std::exp2(lq);
        const float A = std::pow(10.f, gain / 40.f);
        const float t = std::tan(kPi * fc / fs);
        onePole = false; nst = 1;
        auto setSvf = [&](int i, float g, float k, float c0, float c1, float c2) {
            a1[i] = 1.f / (1.f + g * (g + k)); a2[i] = g * a1[i]; a3[i] = g * a2[i]; m0[i] = c0; m1[i] = c1; m2[i] = c2;
        };
        switch (curShape) {
        case kBell: { const float k = 1.f / (q * A); setSvf(0, t, k, 1.f, k * (A * A - 1.f), 0.f); break; }
        case kLowShelf: { const float k = 1.f / q; setSvf(0, t / std::sqrt(A), k, 1.f, k * (A - 1.f), A * A - 1.f); break; }
        case kHighShelf: { const float k = 1.f / q; setSvf(0, t * std::sqrt(A), k, A * A, k * (1.f - A) * A, 1.f - A * A); break; }
        default: {
            const bool hp = curShape == kLowCut;
            if (curSlope == 0) { onePole = true; G = t / (1.f + t); break; }
            nst = cutStages(curSlope); const float* kb = bwK(nst);
            for (int i = 0; i < nst; ++i) {
                const float k = i == nst - 1 ? kb[i] * 0.70710678f / q : kb[i];
                if (hp) setSvf(i, t, k, 1.f, -k, -1.f); else setSvf(i, t, k, 0.f, 0.f, 1.f);
            }
        }
        }
    }
    inline float tick(float x, int c) {
        if (onePole) {
            const float v = (x - op[c]) * G, lp = v + op[c]; op[c] = lp + v;
            return curShape == kLowCut ? x - lp : lp;
        }
        for (int i = 0; i < nst; ++i) {
            const float v3 = x - ic2[c][i];
            const float v1 = a1[i] * ic1[c][i] + a2[i] * v3;
            const float v2 = ic2[c][i] + a2[i] * ic1[c][i] + a3[i] * v3;
            ic1[c][i] = 2.f * v1 - ic1[c][i]; ic2[c][i] = 2.f * v2 - ic2[c][i];
            x = m0[i] * x + m1[i] * v1 + m2[i] * v2;
        }
        return x;
    }
    bool applies(int c) const { return curPlace == kPlaceBoth || (curPlace == kPlaceA && c == 0) || (curPlace == kPlaceB && c == 1); }
};

// ------------------------------------------------------------------ engine
class Engine {
public:
    float p[kParamCount];

    Engine() {
        std::fill(p, p + kParamCount, 0.f);
        p[kProcMode] = 0; p[kLinQuality] = 1; p[kChannelMode] = 0; p[kOutGain] = 0; p[kAnalyzer] = 2; p[kRange] = 1; p[kMidiOn] = 1;
        for (int b = 0; b < kNumBands; ++b) defaultBand(b, 1000.f);
        std::fill(ana, ana + kAnaSize, 0.f);
    }
    void defaultBand(int b, float hz) {
        p[bp(b, kBUsed)] = 0; p[bp(b, kBOn)] = 1; p[bp(b, kBShape)] = kBell; p[bp(b, kBFreq)] = hz;
        p[bp(b, kBGain)] = 0; p[bp(b, kBQ)] = 1; p[bp(b, kBSlope)] = 1; p[bp(b, kBPlace)] = kPlaceBoth;
    }
    void setSampleRate(float sr) {
        fs = sr;
        smooth = 1.f - std::exp(-1.f / (0.015f * sr * 1.f / kCtl));
        fadeStep = (float)kCtl / (0.008f * sr);
        holdTicks = (int)(0.03f * sr / kCtl);
        reset();
    }
    void reset() {
        for (Band& b : bands) { b.reset(); syncTargets(b, &b - bands); b.curShape = b.shape; b.curSlope = b.slope; b.curPlace = b.place;
            b.oct = std::log2(clampf(p[bp(int(&b - bands), kBFreq)], 10.f, 30000.f)); b.gain = p[bp(int(&b - bands), kBGain)];
            b.lq = std::log2(clampf(p[bp(int(&b - bands), kBQ)], 0.025f, 40.f)); b.mix = b.m = (b.used && b.on) ? 1.f : 0.f; b.dm = 0; b.design(fs); }
        sOut = dbToGain(p[kOutGain]); ctl = 0;
        setupLinear(true);
    }

    // latency (samples) the host must compensate
    uint32_t latency() const { return linear ? (uint32_t)(firN + firN / 2) : 0; }

    // spectrum analyser feed: a ring of mono samples (pre or post EQ), read by the UI
    static constexpr int kAnaSize = 16384;
    float ana[kAnaSize];
    struct AtomicIndex {          // copyable wrapper so the engine itself can be copied (tests)
        std::atomic<uint32_t> v { 0 };
        AtomicIndex() {}
        AtomicIndex(const AtomicIndex& o) : v(o.v.load()) {}
        AtomicIndex& operator=(const AtomicIndex& o) { v.store(o.v.load()); return *this; }
        uint32_t load(std::memory_order m = std::memory_order_seq_cst) const { return v.load(m); }
        void store(uint32_t x, std::memory_order m = std::memory_order_seq_cst) { v.store(x, m); }
    } anaWrite;
    float sampleRate() const { return fs; }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) {
        const bool wantLinear = p[kProcMode] > 0.5f;
        const int q = (int)(p[kLinQuality] + 0.5f);
        if (wantLinear != linear || (linear && q != linQ)) { linear = wantLinear; linQ = q; setupLinear(true); }
        const bool ms = p[kChannelMode] > 0.5f;
        const int anaMode = (int)(p[kAnalyzer] + 0.5f);
        const float tOut = dbToGain(p[kOutGain]);
        uint32_t w = anaWrite.load(std::memory_order_relaxed);

        for (uint32_t i = 0; i < n; ++i) {
            if (--ctl <= 0) { ctl = kCtl; if (!linear) controlUpdate(); }
            float a = inL[i], b = inR[i];
            if (anaMode == 1) { ana[w & (kAnaSize - 1)] = 0.5f * (a + b); ++w; }
            if (ms) { const float m = 0.5f * (a + b), s = 0.5f * (a - b); a = m; b = s; }
            float y[2];
            if (!linear) {
                float x[2] = { a, b };
                for (Band& bd : bands) {
                    if (!bd.used) continue;                           // used bands always run, so switching them on is click-free
                    bd.m += bd.dm;
                    for (int c = 0; c < 2; ++c) {
                        if (!bd.applies(c)) continue;
                        const float f = bd.tick(x[c], c);
                        if (bd.m > 0.f) x[c] += (f - x[c]) * bd.m;
                    }
                }
                y[0] = x[0]; y[1] = x[1];
            } else {
                linIn[0][linPos] = a; linIn[1][linPos] = b;
                y[0] = linOut[0][linPos]; y[1] = linOut[1][linPos];
                if (++linPos == firN) { linPos = 0; linearBlock(); }
            }
            if (ms) { const float m = y[0], s = y[1]; y[0] = m + s; y[1] = m - s; }
            sOut += (tOut - sOut) * 0.002f;
            y[0] *= sOut; y[1] *= sOut;
            if (!(std::fabs(y[0]) < 1e6f)) y[0] = 0;
            if (!(std::fabs(y[1]) < 1e6f)) y[1] = 0;
            if (anaMode == 2) { ana[w & (kAnaSize - 1)] = 0.5f * (y[0] + y[1]); ++w; }
            outL[i] = y[0]; outR[i] = y[1];
        }
        anaWrite.store(w, std::memory_order_release);
        for (Band& bd : bands) for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k)
            if (!(std::fabs(bd.ic1[c][k]) < 1e6f) || !(std::fabs(bd.ic2[c][k]) < 1e6f)) bd.reset();
    }

    // combined response of the current settings at one frequency, for channel A (0) or B (1)
    cpx response(float hz, int c) const {
        cpx h(1.f, 0.f);
        for (int b = 0; b < kNumBands; ++b) {
            if (p[bp(b, kBUsed)] < 0.5f || p[bp(b, kBOn)] < 0.5f) continue;
            const int pl = (int)(p[bp(b, kBPlace)] + 0.5f);
            if (!(pl == kPlaceBoth || (pl == kPlaceA && c == 0) || (pl == kPlaceB && c == 1))) continue;
            h *= bandResponse((int)(p[bp(b, kBShape)] + .5f), (int)(p[bp(b, kBSlope)] + .5f), p[bp(b, kBFreq)], p[bp(b, kBGain)], p[bp(b, kBQ)], hz);
        }
        return h;
    }

private:
    static constexpr int kCtl = 16;

    void syncTargets(Band& b, long idx) {
        const int i = (int)idx;
        b.used = p[bp(i, kBUsed)] > 0.5f; b.on = p[bp(i, kBOn)] > 0.5f;
        b.shape = (int)(p[bp(i, kBShape)] + 0.5f); b.slope = (int)(p[bp(i, kBSlope)] + 0.5f); b.place = (int)(p[bp(i, kBPlace)] + 0.5f);
    }
    // every kCtl samples: smooth controls, handle structural changes with a short fade, rebuild coefficients
    void controlUpdate() {
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
            if (!b.used) { b.reset(); b.m = b.mix = 0; b.dm = 0; continue; }
            const float tOct = std::log2(clampf(p[bp(i, kBFreq)], 10.f, 30000.f)), tG = p[bp(i, kBGain)], tQ = std::log2(clampf(p[bp(i, kBQ)], 0.025f, 40.f));
            if (b.mix <= 0.f && b.hold > 0) { b.oct = tOct; b.gain = tG; b.lq = tQ; }
            else { b.oct += (tOct - b.oct) * smooth; b.gain += (tG - b.gain) * smooth; b.lq += (tQ - b.lq) * smooth; }
            b.design(fs);
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
        if (clear && linear) { for (int c = 0; c < 2; ++c) buildKernel(c, kernel[c]); snapshotParams(); }
    }
    static int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
    void snapshotParams() { for (uint32_t i = kChannelMode; i < kParamCount; ++i) designed[i] = p[i]; }
    bool paramsChanged() const {
        for (uint32_t i = kBandFirst; i <= kBandLast; ++i) if (designed[i] != p[i]) return true;
        return false;
    }
    // FIR (length firN, linear phase) from the analog magnitude response, as a 2N-point spectrum for overlap-add
    void buildKernel(int c, std::vector<cpx>& K) {
        const int N = firN;
        for (int k = 0; k <= N / 2; ++k) {
            const float hz = std::max(1.f, (float)k * fs / N);
            const float mag = std::min(std::abs(response(hz, c)), 1000.f);
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
                ifft2(work2);
            }
            for (int i = 0; i < 2 * N; ++i) work[i] *= kernel[c][i];
            ifft2(work);
            for (int i = 0; i < N; ++i) {
                float v = work[i].real();
                if (changed) { const float t = (float)i / N; v = work2[i].real() * (1.f - t) + v * t; }
                linOut[c][i] = v + overlap[c][i];
                overlap[c][i] = work[i + N].real();
            }
        }
    }
    void ifft2(std::vector<cpx>& v) { fft.run(v.data(), true); }

    Band bands[kNumBands];
    float fs = 48000.f, smooth = 0.1f, fadeStep = 0.05f, sOut = 1.f; int holdTicks = 90;
    int ctl = 0;
    bool linear = false; int linQ = 1, firN = 4096, linPos = 0;
    Fft fft, ifft;
    std::vector<float> linIn[2], linOut[2], overlap[2], designed;
    std::vector<cpx> kernel[2], kernelOld[2], work, work2, design;
};

} // namespace mica
