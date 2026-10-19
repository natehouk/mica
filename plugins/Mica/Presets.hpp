// Factory presets (each lists only what it changes from the engine defaults: no bands, zero latency). License: MIT.
#pragma once
#include "MicaEngine.hpp"
#include <algorithm>

namespace mica {

struct PV { uint32_t id; float v; };
struct PresetDef { const char* name; const char* category; const PV* pv; };
struct Preset { const char* name; const char* category; float v[kParamCount]; };

static const char* const kCategories[] = { "Vocals", "Drums", "Bass & Instruments", "Mix & Master", "Corrective" };
static constexpr int kNumCategories = 5;

// B(band, shape, freq, gain, q, slope, place)
#define B(n, sh, f, g, q, sl, pl) {bp(n,kBUsed),1},{bp(n,kBOn),1},{bp(n,kBShape),sh},{bp(n,kBFreq),f},{bp(n,kBGain),g},{bp(n,kBQ),q},{bp(n,kBSlope),sl},{bp(n,kBPlace),pl}
#define BELL(n, f, g, q) B(n, kBell, f, g, q, 1, 0)
#define LS(n, f, g) B(n, kLowShelf, f, g, 0.707f, 1, 0)
#define HS(n, f, g) B(n, kHighShelf, f, g, 0.707f, 1, 0)
#define LC(n, f, sl) B(n, kLowCut, f, 0, 0.707f, sl, 0)
#define HC(n, f, sl) B(n, kHighCut, f, 0, 0.707f, sl, 0)
#define END { kParamCount, 0 }
namespace pv {
static const PV init[] = { END };
// ---------------------------------------------------------------- Vocals
static const PV vocalClarity[] = { LC(0, 90, 2), BELL(1, 250, -2.5f, 1.2f), BELL(2, 3200, 2.5f, 1.f), HS(3, 10000, 3), END };
static const PV vocalWarm[]    = { LC(0, 70, 2), BELL(1, 200, 2, 0.8f), BELL(2, 2500, -1.5f, 1.5f), HS(3, 12000, 1.5f), END };
static const PV vocalAir[]     = { LC(0, 100, 2), HS(1, 12000, 5), BELL(2, 5000, 1, 0.8f), END };
static const PV deHarsh[]      = { LC(0, 80, 2), BELL(1, 2800, -3, 3), BELL(2, 6500, -3, 4), END };
static const PV podcast[]      = { LC(0, 80, 3), BELL(1, 300, -3, 1), BELL(2, 2500, 3, 0.9f), HC(3, 15000, 2), END };
static const PV backing[]      = { LC(0, 180, 2), BELL(1, 2000, -2, 0.8f), HS(2, 9000, 2), END };
// ---------------------------------------------------------------- Drums
static const PV kickPunch[]    = { LC(0, 30, 3), BELL(1, 60, 4, 1.4f), BELL(2, 350, -5, 1.5f), BELL(3, 4000, 4, 1.2f), END };
static const PV snareCrack[]   = { LC(0, 80, 2), BELL(1, 200, 3, 1.2f), BELL(2, 900, -3, 2), BELL(3, 5000, 3.5f, 1), END };
static const PV overheads[]    = { LC(0, 250, 2), BELL(1, 600, -2, 1.f), HS(2, 10000, 3), END };
static const PV drumBus[]      = { LS(0, 80, 2), BELL(1, 400, -2, 0.9f), HS(2, 8000, 2), END };
static const PV hats[]         = { LC(0, 400, 3), BELL(1, 8000, 2, 1), END };
static const PV toms[]         = { LC(0, 60, 2), BELL(1, 110, 3, 1.2f), BELL(2, 450, -4, 2), BELL(3, 3500, 3, 1.4f), END };
// ---------------------------------------------------------------- Bass & Instruments
static const PV bassWeight[]   = { LC(0, 30, 3), LS(1, 90, 3), BELL(2, 250, -2, 1.2f), BELL(3, 900, 2, 1.5f), END };
static const PV bassDefine[]   = { LC(0, 40, 2), BELL(1, 700, 3, 1.4f), BELL(2, 2500, 2, 1.4f), HC(3, 6000, 1), END };
static const PV acousticGtr[]  = { LC(0, 90, 2), BELL(1, 200, -3, 1.2f), BELL(2, 3000, 2, 0.9f), HS(3, 10000, 2), END };
static const PV electricGtr[]  = { LC(0, 100, 2), BELL(1, 400, -2, 1.5f), BELL(2, 1800, 2.5f, 1.2f), HC(3, 9000, 2), END };
static const PV piano[]        = { LC(0, 40, 1), BELL(1, 300, -2, 1), HS(2, 8000, 2), END };
static const PV synthPad[]     = { LC(0, 150, 2), BELL(1, 500, -2.5f, 0.8f), HS(2, 7000, 2), END };
// ---------------------------------------------------------------- Mix & Master
static const PV smile[]        = { LS(0, 100, 2.5f), BELL(1, 1500, -1, 0.5f), HS(2, 9000, 2.5f), END };
static const PV masterGentle[] = { LC(0, 25, 3), LS(1, 60, 1), BELL(2, 300, -0.8f, 0.8f), HS(3, 12000, 1.2f), {kProcMode,1}, END };
static const PV masterBright[] = { LC(0, 25, 3), BELL(1, 3500, 1, 0.7f), HS(2, 14000, 2), {kProcMode,1}, END };
static const PV msWidth[]      = { B(0, kLowCut, 150, 0, 0.707f, 2, 2), B(1, kHighShelf, 6000, 2.5f, 0.707f, 1, 2), {kChannelMode,1}, END };
static const PV monoBass[]     = { B(0, kLowCut, 120, 0, 0.707f, 3, 2), {kChannelMode,1}, {kProcMode,1}, END };
static const PV loudness[]     = { LS(0, 80, 3), BELL(1, 3000, 1.5f, 0.6f), HS(2, 12000, 2), {kOutGain,-3}, END };
// ---------------------------------------------------------------- Corrective
static const PV rumble[]       = { LC(0, 40, 3), END };
static const PV hum50[]        = { BELL(0, 50, -24, 30), BELL(1, 100, -18, 30), BELL(2, 150, -12, 30), BELL(3, 200, -9, 30), END };
static const PV hum60[]        = { BELL(0, 60, -24, 30), BELL(1, 120, -18, 30), BELL(2, 180, -12, 30), BELL(3, 240, -9, 30), END };
static const PV mud[]          = { BELL(0, 300, -4, 1.4f), END };
static const PV boxy[]         = { BELL(0, 500, -4, 2), END };
static const PV telephone[]    = { LC(0, 400, 3), HC(1, 3200, 3), BELL(2, 1500, 4, 1), END };
} // namespace pv
#undef B
#undef BELL
#undef LS
#undef HS
#undef LC
#undef HC
#undef END

static const PresetDef kPresetDefs[] = {
    {"Init", "Init", pv::init},
    {"Vocal Clarity", "Vocals", pv::vocalClarity}, {"Warm Vocal", "Vocals", pv::vocalWarm}, {"Vocal Air", "Vocals", pv::vocalAir},
    {"De-Harsh", "Vocals", pv::deHarsh}, {"Podcast Voice", "Vocals", pv::podcast}, {"Backing Vocals", "Vocals", pv::backing},
    {"Kick Punch", "Drums", pv::kickPunch}, {"Snare Crack", "Drums", pv::snareCrack}, {"Overheads", "Drums", pv::overheads},
    {"Drum Bus", "Drums", pv::drumBus}, {"Hi-Hats", "Drums", pv::hats}, {"Toms", "Drums", pv::toms},
    {"Bass Weight", "Bass & Instruments", pv::bassWeight}, {"Bass Definition", "Bass & Instruments", pv::bassDefine}, {"Acoustic Guitar", "Bass & Instruments", pv::acousticGtr},
    {"Electric Guitar", "Bass & Instruments", pv::electricGtr}, {"Piano", "Bass & Instruments", pv::piano}, {"Synth Pad", "Bass & Instruments", pv::synthPad},
    {"Smile Curve", "Mix & Master", pv::smile}, {"Gentle Master", "Mix & Master", pv::masterGentle}, {"Bright Master", "Mix & Master", pv::masterBright},
    {"Wider Sides", "Mix & Master", pv::msWidth}, {"Mono Low End", "Mix & Master", pv::monoBass}, {"Loudness Curve", "Mix & Master", pv::loudness},
    {"Rumble Filter", "Corrective", pv::rumble}, {"Hum Remover 50 Hz", "Corrective", pv::hum50}, {"Hum Remover 60 Hz", "Corrective", pv::hum60},
    {"Mud Cut", "Corrective", pv::mud}, {"Boxiness", "Corrective", pv::boxy}, {"Telephone", "Corrective", pv::telephone},
};
static constexpr uint32_t kNumPresets = sizeof(kPresetDefs) / sizeof(kPresetDefs[0]);

inline const Preset* presets() {
    static Preset table[kNumPresets];
    static bool done = false;
    if (done) return table;
    done = true;
    Engine e;
    for (uint32_t i = 0; i < kNumPresets; ++i) {
        table[i].name = kPresetDefs[i].name; table[i].category = kPresetDefs[i].category;
        std::copy(e.p, e.p + kParamCount, table[i].v);
        for (const PV* x = kPresetDefs[i].pv; x->id != kParamCount; ++x) table[i].v[x->id] = x->v;
    }
    return table;
}

} // namespace mica
