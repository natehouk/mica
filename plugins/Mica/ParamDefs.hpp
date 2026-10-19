// Parameter metadata shared by the DSP wrapper and the UI. License: MIT.
#pragma once
#include "MicaEngine.hpp"
#include <cstdio>

namespace mica {

static constexpr uint32_t kHintInteger = 1, kHintBoolean = 2, kHintLog = 4, kHintOutput = 8, kHintNoAuto = 16;
struct ParamDef { const char* name; const char* sym; const char* unit; float min, max; uint32_t hints; };

inline const ParamDef& paramDef(uint32_t i) {
    static ParamDef t[kParamCount];
    static bool done = false;
    if (!done) {
        done = true;
        auto set = [](uint32_t i, const char* n, const char* s, const char* u, float lo, float hi, uint32_t h) { t[i] = { n, s, u, lo, hi, h }; };
        const uint32_t I = kHintInteger, B = kHintBoolean | kHintInteger;
        set(kProcMode, "Processing Mode", "procmode", "", 0, 1, I);
        set(kLinQuality, "Linear Phase Quality", "linq", "", 0, 2, I);
        set(kChannelMode, "Channel Mode", "chmode", "", 0, 1, I);
        set(kOutGain, "Output Gain", "outgain", "dB", -24, 24, 0);
        set(kAnalyzer, "Analyzer", "analyzer", "", 0, 2, I | kHintNoAuto);
        set(kRange, "Display Range", "range", "", 0, 2, I | kHintNoAuto);
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
                case kBSlope: set(i, nm[b][f], sy[b][f], "", 0, 3, I); break;
                default: set(i, nm[b][f], sy[b][f], "", 0, 2, I); break;
                }
            }
        set(kOutLearn, "MIDI Learn", "o_learn", "", 0, 10000000, kHintOutput);
    }
    return t[i];
}

inline int bandOf(uint32_t p) { return (p >= kBandFirst && p <= kBandLast) ? (int)((p - kBandFirst) / kBandFields) : -1; }
inline int fieldOf(uint32_t p) { return (p >= kBandFirst && p <= kBandLast) ? (int)((p - kBandFirst) % kBandFields) : -1; }

inline const char* const* enumLabels(uint32_t p, int& count) {
    static const char* const mode[] = { "Zero Latency", "Linear Phase" };
    static const char* const lq[] = { "Low", "Medium", "High" };
    static const char* const chm[] = { "Stereo", "Mid/Side" };
    static const char* const ana[] = { "Off", "Pre EQ", "Post EQ" };
    static const char* const range[] = { "6 dB", "12 dB", "30 dB" };
    static const char* const shape[] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut" };
    static const char* const slope[] = { "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct" };
    static const char* const place[] = { "Stereo", "Left / Mid", "Right / Side" };
    switch (p) {
    case kProcMode: count = 2; return mode;
    case kLinQuality: count = 3; return lq;
    case kChannelMode: count = 2; return chm;
    case kAnalyzer: count = 3; return ana;
    case kRange: count = 3; return range;
    default: break;
    }
    switch (fieldOf(p)) {
    case kBShape: count = kNumShapes; return shape;
    case kBSlope: count = 4; return slope;
    case kBPlace: count = 3; return place;
    default: count = 0; return nullptr;
    }
}
inline float rangeDb(float v) { const int r = (int)(v + 0.5f); return r == 0 ? 6.f : (r == 1 ? 12.f : 30.f); }

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
