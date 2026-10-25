// Parameter metadata shared by the DSP wrapper and the UI. License: MIT.
#pragma once
#include "MicaEngine.hpp"
#include <cstdio>

namespace mica {

static constexpr uint32_t kHintInteger = 1, kHintBoolean = 2, kHintLog = 4, kHintOutput = 8, kHintNoAuto = 16, kHintView = 32;
// kHintView: a display setting (analyser, piano, ...) — not part of undo, A/B or presets
struct ParamDef { const char* name; const char* sym; const char* unit; float min, max; uint32_t hints; };

inline const ParamDef& paramDef(uint32_t i) {
    static ParamDef t[kParamCount];
    static bool done = false;
    if (!done) {
        done = true;
        auto set = [](uint32_t i, const char* n, const char* s, const char* u, float lo, float hi, uint32_t h) { t[i] = { n, s, u, lo, hi, h }; };
        const uint32_t I = kHintInteger, B = kHintBoolean | kHintInteger, V = kHintNoAuto | kHintView;
        set(kProcMode, "Processing Mode", "procmode", "", 0, kNumProcModes - 1, I);
        set(kLinQuality, "Linear Phase Quality", "linq", "", 0, 2, I);
        set(kChannelMode, "Channel Mode", "chmode", "", 0, 1, I);
        set(kOutGain, "Output Gain", "outgain", "dB", -24, 24, 0);
        set(kAnalyzer, "Analyzer", "analyzer", "", 0, 3, I | V);
        set(kRange, "Display Range", "range", "", 0, 3, I | V);
        set(kMidiOn, "MIDI", "midion", "", 0, 1, B);
        static char nm[kNumBands][kBandFields][28], sy[kNumBands][kBandFields][16];
        static const char* fn[kBandFields] = { "Used", "Enabled", "Shape", "Frequency", "Gain", "Q", "Slope", "Channel" };
        static const char* fs[kBandFields] = { "used", "on", "shape", "freq", "gain", "q", "slope", "chan" };
        for (int b = 0; b < kNumBands; ++b)
            for (int f = 0; f < kBandFields; ++f) {
                std::snprintf(nm[b][f], 28, "Band %d %s", b + 1, fn[f]); std::snprintf(sy[b][f], 16, "b%d%s", b + 1, fs[f]);
                const uint32_t i = bp(b, f);
                switch (f) {
                case kBUsed: case kBOn: set(i, nm[b][f], sy[b][f], "", 0, 1, B); break;
                case kBShape: set(i, nm[b][f], sy[b][f], "", 0, kNumShapes - 1, I); break;
                case kBFreq: set(i, nm[b][f], sy[b][f], "Hz", 10, 30000, kHintLog); break;
                case kBGain: set(i, nm[b][f], sy[b][f], "dB", -30, 30, 0); break;
                case kBQ: set(i, nm[b][f], sy[b][f], "", 0.025f, 40, kHintLog); break;
                case kBSlope: set(i, nm[b][f], sy[b][f], "", 0, kNumSlopes - 1, I); break;
                default: set(i, nm[b][f], sy[b][f], "", 0, 2, I); break;
                }
            }
        set(kOutLearn, "MIDI Learn", "o_learn", "", 0, 10000000, kHintOutput);
        // ---- 2.0
        set(kCharacter, "Character", "character", "", 0, kNumCharacters - 1, I);
        set(kAutoGain, "Auto Gain", "autogain", "", 0, 1, B);
        set(kGainScale, "Gain Scale", "gainscale", "%", -100, 200, 0);
        set(kPhaseInvert, "Phase Invert", "invert", "", 0, 1, B);
        set(kAnaExternal, "Analyzer External", "anaext", "", 0, 1, B | V);
        set(kAnaRange, "Analyzer Range", "anarange", "", 0, 2, I | V);
        set(kAnaSpeed, "Analyzer Speed", "anaspeed", "", 0, 4, I | V);
        set(kAnaRes, "Analyzer Resolution", "anares", "", 0, 3, I | V);
        set(kAnaTilt, "Analyzer Tilt", "anatilt", "dB/oct", 0, 6, V);
        set(kAnaFreeze, "Analyzer Freeze", "anafreeze", "", 0, 1, B | V);
        set(kCollision, "Collision Detection", "collision", "", 0, 1, B | V);
        set(kPiano, "Piano Display", "piano", "", 0, 1, B | V);
        static char nm2[kNumBands][kBand2Fields][36], sy2[kNumBands][kBand2Fields][20];
        static const char* fn2[kBand2Fields] = { "Shape Slope", "Dynamic", "Dynamic Range", "Threshold", "Attack", "Release", "Sidechain", "Free Sidechain", "Sidechain Freq", "Sidechain Q", "Spectral" };
        static const char* fs2[kBand2Fields] = { "slope2", "dyn", "dynrange", "thresh", "attack", "release", "sc", "scfree", "scfreq", "scq", "spectral" };
        for (int b = 0; b < kNumBands; ++b)
            for (int f = 0; f < kBand2Fields; ++f) {
                std::snprintf(nm2[b][f], 36, "Band %d %s", b + 1, fn2[f]); std::snprintf(sy2[b][f], 20, "b%d%s", b + 1, fs2[f]);
                const uint32_t i = bp2(b, f);
                switch (f) {
                case kB2Slope: set(i, nm2[b][f], sy2[b][f], "", 0, kNumSlopes - 2, I); break;
                case kB2Dyn: case kB2ScFree: case kB2Spectral: set(i, nm2[b][f], sy2[b][f], "", 0, 1, B); break;
                case kB2Range: set(i, nm2[b][f], sy2[b][f], "dB", -30, 30, 0); break;
                case kB2Thresh: set(i, nm2[b][f], sy2[b][f], "dB", -80, 0, 0); break;
                case kB2Attack: set(i, nm2[b][f], sy2[b][f], "ms", 0.1f, 500, kHintLog); break;
                case kB2Release: set(i, nm2[b][f], sy2[b][f], "ms", 5, 5000, kHintLog); break;
                case kB2Source: set(i, nm2[b][f], sy2[b][f], "", 0, 1, I); break;
                case kB2ScFreq: set(i, nm2[b][f], sy2[b][f], "Hz", 10, 30000, kHintLog); break;
                case kB2ScQ: set(i, nm2[b][f], sy2[b][f], "", 0.1f, 40, kHintLog); break;
                default: break;
                }
            }
    }
    return t[i];
}

inline int bandOf(uint32_t p) {
    if (p >= kBandFirst && p <= kBandLast) return (int)((p - kBandFirst) / kBandFields);
    if (p >= kBand2First && p <= kBand2Last) return (int)((p - kBand2First) / kBand2Fields);
    return -1;
}
inline int fieldOf(uint32_t p) { return (p >= kBandFirst && p <= kBandLast) ? (int)((p - kBandFirst) % kBandFields) : -1; }
inline int field2Of(uint32_t p) { return (p >= kBand2First && p <= kBand2Last) ? (int)((p - kBand2First) % kBand2Fields) : -1; }
// a setting saved in presets, undo and A/B (not an output, not a display setting)
inline bool isSetting(uint32_t p) { return p < kParamCount && !(paramDef(p).hints & (kHintOutput | kHintView)); }

inline const char* slopeLabel(int idx) {
    static char b[kNumSlopes][16]; static bool done = false;
    if (!done) { done = true; for (int i = 0; i < kNumSlopes; ++i) { if (slopeDbOf(i) == eq::kBrickwall) std::snprintf(b[i], 16, "Brickwall"); else std::snprintf(b[i], 16, "%d dB/oct", slopeDbOf(i)); } }
    return b[clampi(idx, 0, kNumSlopes - 1)];
}

inline const char* const* enumLabels(uint32_t p, int& count) {
    static const char* const mode[] = { "Zero Latency", "Linear Phase", "Natural Phase" };
    static const char* const lq[] = { "Low", "Medium", "High" };
    static const char* const chm[] = { "Stereo", "Mid/Side" };
    static const char* const ana[] = { "Off", "Pre EQ", "Post EQ", "Pre + Post" };
    static const char* const range[] = { "6 dB", "12 dB", "30 dB", "3 dB" };
    static const char* const shape[] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" };
    static const char* const place[] = { "Stereo", "Left / Mid", "Right / Side" };
    static const char* const chr[] = { "Clean", "Gentle", "Warm" };
    static const char* const arange[] = { "60 dB", "90 dB", "120 dB" };
    static const char* const speed[] = { "Very Slow", "Slow", "Medium", "Fast", "Very Fast" };
    static const char* const res[] = { "Low", "Medium", "High", "Maximum" };
    static const char* const src[] = { "Internal", "External" };
    static const char* slopes[kNumSlopes]; for (int i = 0; i < kNumSlopes; ++i) slopes[i] = slopeLabel(i);
    switch (p) {
    case kProcMode: count = kNumProcModes; return mode;
    case kLinQuality: count = 3; return lq;
    case kChannelMode: count = 2; return chm;
    case kAnalyzer: count = 4; return ana;
    case kRange: count = 4; return range;
    case kCharacter: count = kNumCharacters; return chr;
    case kAnaRange: count = 3; return arange;
    case kAnaSpeed: count = 5; return speed;
    case kAnaRes: count = 4; return res;
    default: break;
    }
    switch (fieldOf(p)) {
    case kBShape: count = kNumShapes; return shape;
    case kBSlope: count = kNumSlopes; return slopes;
    case kBPlace: count = 3; return place;
    default: break;
    }
    switch (field2Of(p)) {
    case kB2Slope: count = kNumSlopes - 1; return slopes;
    case kB2Source: count = 2; return src;
    default: count = 0; return nullptr;
    }
}
// display range index -> dB (index 3, 3 dB, was added in 2.0)
inline float rangeDb(float v) { static const float t[4] = { 6.f, 12.f, 30.f, 3.f }; return t[clampi((int)(v + 0.5f), 0, 3)]; }

inline float toNorm(uint32_t p, float v) {
    const ParamDef& d = paramDef(p);
    if (d.hints & kHintLog) return std::log(v / d.min) / std::log(d.max / d.min);
    return (v - d.min) / (d.max - d.min);
}
inline float fromNorm(uint32_t p, float n) {
    const ParamDef& d = paramDef(p);
    n = clampf(n, 0.f, 1.f);
    float v = (d.hints & kHintLog) ? d.min * std::pow(d.max / d.min, n) : d.min + n * (d.max - d.min);
    if (d.hints & kHintInteger) v = std::round(v);
    return v;
}

} // namespace mica
