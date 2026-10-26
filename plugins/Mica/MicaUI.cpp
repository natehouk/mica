// Mica — custom vector UI (DPF/NanoVG). Original design. License: MIT.
#include "DistrhoUI.hpp"
#include "MicaPlugin.hpp"
#include "ResizeHandle.hpp"
#include "Help.hpp"
#include "common/CurveFit.hpp"
#include "common/Platform.hpp"
#include "common/UiKit.hpp"
#include "common/UndoHistory.hpp"
#include "common/UserPresets.hpp"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

using namespace mica;
using DGL_NAMESPACE::ResizeHandle;
using epsilon::nowSec;

static bool gTipsOn = true;
static constexpr float W = 1180.f, H = 720.f;

#define C_BG        Color(0x0f, 0x11, 0x13)
#define C_PANEL     Color(0x18, 0x1b, 0x1e)
#define C_PANEL_HI  Color(0x23, 0x27, 0x2b)
#define C_LINE      Color(0x2d, 0x32, 0x37)
#define C_TEXT      Color(0xe6, 0xea, 0xec)
#define C_DIM       Color(0x88, 0x90, 0x97)
#define C_FAINT     Color(0x4a, 0x51, 0x58)
#define C_ACC       Color(0x9f, 0xd8, 0xe0)       // mica: pale silvery cyan
#define C_ANA       Color(0x5a, 0x7a, 0x8c)
#define C_EXT       Color(0xe8, 0xa8, 0x60)       // external spectrum
#define C_HOT       Color(0xe8, 0x58, 0x50)       // collisions, delete
#define C_DISPLAY   Color(0x0a, 0x0c, 0x0d)
#define C_LEFT      Color(0xf0, 0xd8, 0x90)
#define C_RIGHT     Color(0x9c, 0xc8, 0xf8)
#define C_FAV       Color(0xff, 0xc8, 0x5a)       // favourite star

static Color withAlpha(const Color& c, float a) { Color r(c); r.alpha = a; return r; }
static Color bandColor(int b) {
    const float h = std::fmod(b * 0.1618034f * 6.f, 6.f), x = 1.f - std::fabs(std::fmod(h, 2.f) - 1.f);
    float r = 0, g = 0, bl = 0;
    if (h < 1) { r = 1; g = x; } else if (h < 2) { r = x; g = 1; } else if (h < 3) { g = 1; bl = x; } else if (h < 4) { g = x; bl = 1; } else if (h < 5) { r = x; bl = 1; } else { r = 1; bl = x; }
    return Color(0.45f + 0.5f * r, 0.45f + 0.5f * g, 0.45f + 0.5f * bl);
}
static epsilon::Theme micaTheme() {
    return { C_BG, C_PANEL, C_PANEL_HI, C_LINE, C_TEXT, C_DIM, C_FAINT, C_ACC, Color(0x1d, 0x21, 0x25), Color(0xc6, 0xcc, 0xd0) };
}
static const char* noteName(float hz, char* b, size_t n) {
    static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float m = 69.f + 12.f * std::log2(std::max(hz, 1.f) / 440.f);
    const int k = (int)std::lround(m); const int cents = (int)std::lround((m - k) * 100.f);
    snprintf(b, n, "%s%d %+dc", names[((k % 12) + 12) % 12], k / 12 - 1, cents);
    return b;
}

// ---------------------------------------------------------------- layout
static constexpr float DX = 16, DY = 64, DW = 1104, DH = 420;            // EQ display
static constexpr float MX = 1128, MW = 36;                                // output meter
static constexpr float PL = DX + 30, PW = DW - 44;                        // plot area (x)
static constexpr float PY = 494, PH = 210;                                // bottom panels
static constexpr float BX = 16, BW = 724, GX = 748, GW = 416;             // band panel, global panel
static constexpr float SEG_H = 22;
static constexpr float FMIN = 10.f, FMAX = 30000.f;
static constexpr float AX = DX + 186, AY = DY + 32, AW = 330, AH = 214;   // analyser menu
static constexpr float TB_Y = DY + 8, TB_H = 20;                          // display toolbar
static constexpr float SK_X = DX + 34, SK_W = 64, MA_X = SK_X + 70, MA_W = 60, AN_X = MA_X + 66, AN_W = 96;
static constexpr float CH_X = AN_X + AN_W + 112;                         // MIDI Learn: active-band chips
static constexpr float MPW = 420, MPH = 128, MPX = DX + DW / 2 - MPW / 2, MPY = DY + 40;   // EQ Match panel
static constexpr int kFieldSlope = 100;                                   // pseudo band field: the shape's slope

enum WType { Knob, BandToggle, Switch, Segment, DelButton, Stepper, Hold };
enum Group { GBand, GDyn, GGlobal, GAna };
// field >= 0: a 1.x band field of the selected band; field2 >= 0: a 2.0 band field; otherwise a global param
struct Ctl { WType type; int field, field2; uint32_t param; float x, y, r; const char* label; int group; };

static std::vector<Ctl> buildControls() {
    std::vector<Ctl> c;
    const uint32_t N = kParamCount;
    // band panel
    c.push_back({ BandToggle, kBOn, -1, N, BX + 30, PY + 24, 0, "", GBand });
    c.push_back({ Segment, kBShape, -1, N, BX + 415, PY + 24, 530, "", GBand });
    c.push_back({ DelButton, -1, -1, N, BX + BW - 22, PY + 24, 0, "", GBand });
    c.push_back({ Stepper, kFieldSlope, -1, N, BX + 104, PY + 62, 148, "SLOPE", GBand });
    c.push_back({ Segment, kBPlace, -1, N, BX + 316, PY + 62, 228, "", GBand });
    c.push_back({ Hold, -1, -1, N, BX + 476, PY + 62, 64, "SOLO", GBand });
    c.push_back({ Switch, -1, kB2Dyn, N, BX + 548, PY + 62, 0, "DYNAMIC", GBand });
    c.push_back({ Knob, kBFreq, -1, N, BX + 66, PY + 140, 26, "FREQUENCY", GBand });
    c.push_back({ Knob, kBGain, -1, N, BX + 166, PY + 140, 26, "GAIN", GBand });
    c.push_back({ Knob, kBQ, -1, N, BX + 266, PY + 140, 26, "Q", GBand });
    c.push_back({ Segment, -1, kB2Source, N, BX + 404, PY + 104, 116, "", GDyn });
    c.push_back({ Switch, -1, kB2ScFree, N, BX + 486, PY + 104, 0, "FREE SC", GDyn });
    c.push_back({ Switch, -1, kB2Spectral, N, BX + 604, PY + 104, 0, "SPECTRAL", GDyn });
    c.push_back({ Knob, -1, kB2Range, N, BX + 372, PY + 158, 17, "RANGE", GDyn });
    c.push_back({ Knob, -1, kB2Thresh, N, BX + 432, PY + 158, 17, "THRESH", GDyn });
    c.push_back({ Knob, -1, kB2Attack, N, BX + 492, PY + 158, 17, "ATTACK", GDyn });
    c.push_back({ Knob, -1, kB2Release, N, BX + 552, PY + 158, 17, "RELEASE", GDyn });
    c.push_back({ Knob, -1, kB2ScFreq, N, BX + 618, PY + 158, 17, "SC FREQ", GDyn });
    c.push_back({ Knob, -1, kB2ScQ, N, BX + 678, PY + 158, 17, "SC Q", GDyn });
    // global panel
    c.push_back({ Segment, -1, -1, kProcMode, GX + 146, PY + 36, 268, "", GGlobal });
    c.push_back({ Segment, -1, -1, kLinQuality, GX + 345, PY + 36, 116, "", GGlobal });
    c.push_back({ Segment, -1, -1, kChannelMode, GX + 94, PY + 88, 164, "", GGlobal });
    c.push_back({ Segment, -1, -1, kCharacter, GX + 293, PY + 88, 214, "", GGlobal });
    c.push_back({ Segment, -1, -1, kAnalyzer, GX + 94, PY + 140, 164, "", GGlobal });
    c.push_back({ Segment, -1, -1, kRange, GX + 252, PY + 140, 136, "", GGlobal });
    c.push_back({ Knob, -1, -1, kGainScale, GX + 344, PY + 146, 19, "SCALE", GGlobal });
    c.push_back({ Knob, -1, -1, kOutGain, GX + 392, PY + 146, 19, "OUTPUT", GGlobal });
    c.push_back({ Switch, -1, -1, kAutoGain, GX + 28, PY + 188, 0, "AUTO GAIN", GGlobal });
    c.push_back({ Switch, -1, -1, kPhaseInvert, GX + 156, PY + 188, 0, "INVERT", GGlobal });
    // analyser menu
    c.push_back({ Switch, -1, -1, kAnaExternal, AX + 26, AY + 26, 0, "EXTERNAL", GAna });
    c.push_back({ Switch, -1, -1, kCollision, AX + 176, AY + 26, 0, "COLLISION", GAna });
    c.push_back({ Switch, -1, -1, kAnaFreeze, AX + 26, AY + 56, 0, "FREEZE", GAna });
    c.push_back({ Switch, -1, -1, kPiano, AX + 176, AY + 56, 0, "PIANO", GAna });
    c.push_back({ Segment, -1, -1, kAnaRange, AX + 210, AY + 96, 216, "RANGE", GAna });
    c.push_back({ Segment, -1, -1, kAnaSpeed, AX + 210, AY + 128, 216, "SPEED", GAna });
    c.push_back({ Segment, -1, -1, kAnaRes, AX + 210, AY + 160, 216, "RESOLUTION", GAna });
    c.push_back({ Stepper, -1, -1, kAnaTilt, AX + 210, AY + 192, 216, "TILT", GAna });
    return c;
}
static const std::vector<Ctl>& controls() { static std::vector<Ctl> c = buildControls(); return c; }

static constexpr float PS_X = 300.f, PS_Y = 20.f, PS_W = 250.f, PS_H = 28.f, PS_ARROW = 30.f;
static constexpr float A_X = 562.f, AB_W = 30.f, COPY_X = 626.f, COPY_W = 44.f;
static constexpr float UNDO_X = 684.f, REDO_X = 738.f, UR_W = 50.f, LEARN_X = 798.f, LEARN_W = 66.f, MIDI_X = 870.f, MIDI_W = 52.f, HELP_X = 928.f, HELP_W = 48.f;
static constexpr float PM_COLW = 160.f, PM_ROW = 23.f, PM_HEAD = 22.f, PM_PAD = 8.f;
static constexpr int ANA_MAX = 16384, COLS = 360, MATCH_N = 8192, NFIT = 120;

class MicaUI : public UI {
public:
    MicaUI() : UI((uint)W, (uint)H), theme(micaTheme()), help("MICA HELP", kHelpPages, kNumHelpPages, kManualURL), store("Mica", ".mica"), resizeHandle(this) {
        loadSharedResources();
        Engine def;
        for (uint32_t i = 0; i < kParamCount; ++i) { val[i] = def.p[i]; defv[i] = def.p[i]; const ParamDef& d = paramDef(i); pparams[i] = { d.sym, d.min, d.max, isSetting(i) }; }
        for (int& m : paramCC) m = -1;
        for (int& m : activeCC) m = -1;
        const double sf = getScaleFactor();
        if (sf != 1.0) setSize((uint)(W * sf), (uint)(H * sf));
        setGeometryConstraints((uint)(W * 0.6 * sf), (uint)(H * 0.6 * sf), true);
        if (isResizable()) resizeHandle.hide();
        userPresets = store.list();
        history.init(kParamCount, [](uint32_t p) { return isSetting(p); });
        history.reset(val);
        help.w = 800; help.h = 540; help.centre(W, H);
        buf.resize(ANA_MAX);
        for (int s = 0; s < 3; ++s) { spec[s].assign(ANA_MAX / 2 + 1, -150.f); col[s].assign(COLS + 1, -150.f); }
        matchPre.assign(MATCH_N / 2 + 1, 0.0); matchSc.assign(MATCH_N / 2 + 1, 0.0);
        sketchDb.assign(COLS + 1, NAN);
        for (bool& s : sel) s = false;
    }

protected:
    float uiScale() const { return (float)getWidth() / W; }

    // ---- values -------------------------------------------------------------
    void setValue(uint32_t p, float v) {
        if (p >= kParamCount) return;
        const ParamDef& d = paramDef(p);
        v = clampf(v, d.min, d.max);
        if (d.hints & kHintInteger) v = std::round(v);
        if (v == val[p]) return;
        val[p] = v; setParameterValue(p, v); repaint();
    }
    void commit(uint32_t p, float v) { if (p >= kParamCount) return; editParameter(p, true); setValue(p, v); editParameter(p, false); }
    void parameterChanged(uint32_t i, float v) override {
        if (i >= kParamCount) return;
        if (i == kOutLearn) {
            if (v != lastLearnEcho && v > 0) {
                const int code = (int)v % 1000000 - 1, prm = code / 128, cc = code % 128;
                for (int& m : paramCC) if (m == cc) m = -1;
                for (int& m : activeCC) if (m == cc) m = -1;
                if (prm >= (int)kParamCount && prm < (int)kParamCount + 3) activeCC[prm - kParamCount] = cc;
                else if (prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
                learnWaiting = -1;
            }
            lastLearnEcho = v; repaint(); return;
        }
        if (val[i] != v) { val[i] = v; history.hostChanged(i, v); repaint(); }
    }
    void stateChanged(const char* key, const char* value) override {
        if (std::strcmp(key, "midimap") != 0) return;
        for (int& m : paramCC) m = -1;
        for (int& m : activeCC) m = -1;
        const char* q = value;
        while (q && *q) {
            int cc = -1, prm = -1;
            if (std::sscanf(q, "%d:%d", &cc, &prm) == 2) {
                if (prm >= MicaPlugin::kActiveTarget && prm < MicaPlugin::kActiveTarget + 3) activeCC[prm - MicaPlugin::kActiveTarget] = cc;
                else if (prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
            }
            const char* s = std::strchr(q, ';'); if (!s) break; q = s + 1;
        }
        repaint();
    }
    bool used(int b) const { return val[bp(b, kBUsed)] > 0.5f; }
    int shapeOf(int b) const { return clampi((int)(val[bp(b, kBShape)] + .5f), 0, kNumShapes - 1); }
    int primary() const { return (focus >= 0 && used(focus)) ? focus : -1; }
    int selCount() const { int n = 0; for (int b = 0; b < kNumBands; ++b) n += sel[b] && used(b); return n; }
    bool dynOn(int b) const { return val[bp2(b, kB2Dyn)] > 0.5f && canDynamic(shapeOf(b)); }

    // ---- undo / redo / A-B -----------------------------------------------------------
    void beginGesture() { gesture = true; history.begin(val); }
    void endGesture() { gesture = false; history.end(val); repaint(); }
    void applyAll(const float* s) { for (uint32_t i = 0; i < kParamCount; ++i) if (isSetting(i) && val[i] != s[i]) commit(i, s[i]); }
    void applySnap(const epsilon::UndoHistory::Snap& s) { applyAll(s.data()); cleanSelection(); }
    void undo() { epsilon::UndoHistory::Snap s; if (history.undo(val, s)) { applySnap(s); flash("Undo"); } }
    void redo() { epsilon::UndoHistory::Snap s; if (history.redo(val, s)) { applySnap(s); flash("Redo"); } }
    void switchAB(int to) { epsilon::UndoHistory::Snap s; if (history.switchTo(to, val, s)) { applySnap(s); flash(to == 0 ? "Setting A" : "Setting B"); } }
    void copyAB() { history.copyToOther(val); flash(history.slot() == 0 ? "Copied A to B" : "Copied B to A"); }
    void cleanSelection() { for (int b = 0; b < kNumBands; ++b) if (!used(b)) sel[b] = false; if (focus >= 0 && !used(focus)) focus = -1; }

    // ---- bands ------------------------------------------------------------------
    void resetBand(int b, float hz) {
        Engine def; def.defaultBand(b, hz);
        for (int f = 0; f < kBandFields; ++f) if (f != kBUsed) commit(bp(b, f), def.p[bp(b, f)]);
        for (int f = 0; f < kBand2Fields; ++f) commit(bp2(b, f), def.p[bp2(b, f)]);
    }
    int newBand(float hz, float db, int shape = kBell, float q = 1.f) {
        int b = -1; for (int i = 0; i < kNumBands; ++i) if (!used(i)) { b = i; break; }
        if (b < 0) { flash("All 24 bands are in use"); return -1; }
        resetBand(b, clampf(hz, FMIN, FMAX));
        commit(bp(b, kBShape), (float)shape); commit(bp(b, kBGain), clampf(db, -30, 30)); commit(bp(b, kBQ), q);
        commit(bp(b, kBSlope), 2); commit(bp2(b, kB2ScFreq), clampf(hz, FMIN, FMAX));
        commit(bp(b, kBOn), 1); commit(bp(b, kBUsed), 1);
        return b;
    }
    void deleteSelected() {
        if (selCount() == 0 && primary() < 0) return;
        beginGesture();
        for (int b = 0; b < kNumBands; ++b) if ((sel[b] || b == focus) && used(b)) { commit(bp(b, kBUsed), 0); commit(bp(b, kBGain), 0); sel[b] = false; }
        endGesture(); focus = -1; repaint();
    }
    void selectOnly(int b) { for (bool& s : sel) s = false; if (b >= 0) sel[b] = true; focus = b; }
    // the slope a band shows: cuts use their 1.x parameter, other shapes the 2.0 one
    int slopeIdx(int b) const { return (int)(val[slopeParam(val, b)] + 0.5f); }
    // change shape and carry the visible slope over to the parameter the new shape uses
    void setShape(int b, int shape) {
        const int oldDb = bandSlopeDb(val, b);
        setValue(bp(b, kBShape), (float)shape);
        const uint32_t sp = slopeParam(val, b);
        const int want = (oldDb == eq::kBrickwall && !isCutShape(shape)) ? 96 : oldDb;
        for (int i = 0; i < kNumSlopes - (isCutShape(shape) ? 0 : 1); ++i) if (slopeDbOf(i) == want) { setValue(sp, (float)i); break; }
    }
    void stepSlope(int b, int dir) {
        const bool cut = isCutShape(shapeOf(b));
        const int maxRank = cut ? kNumSlopes - 1 : kNumSlopes - 2;
        const int r = clampi(slopeRank(slopeIdx(b)) + dir, 0, maxRank);
        setValue(slopeParam(val, b), (float)slopeByRank(r));
    }

    // ---- formatting ---------------------------------------------------------
    void fmtVal(uint32_t p, float v, char* b, size_t n) const {
        int ne = 0;
        if (const char* const* l = enumLabels(p, ne)) { snprintf(b, n, "%s", l[std::max(0, std::min(ne - 1, (int)std::round(v)))]); return; }
        if (paramDef(p).hints & kHintBoolean) { snprintf(b, n, "%s", v > 0.5f ? "On" : "Off"); return; }
        const int f = fieldOf(p), f2 = field2Of(p);
        if (f == kBFreq || f2 == kB2ScFreq) { if (v >= 1000) snprintf(b, n, "%.2f kHz", v / 1000); else snprintf(b, n, v < 100 ? "%.1f Hz" : "%.0f Hz", v); return; }
        if (f == kBGain || p == kOutGain || f2 == kB2Range) { snprintf(b, n, "%+.2f dB", v); return; }
        if (f2 == kB2Thresh) { snprintf(b, n, "%.1f dB", v); return; }
        if (f2 == kB2Attack || f2 == kB2Release) { snprintf(b, n, v < 10 ? "%.1f ms" : "%.0f ms", v); return; }
        if (f == kBQ || f2 == kB2ScQ) { snprintf(b, n, v < 10 ? "%.3f" : "%.1f", v); return; }
        if (p == kGainScale) { snprintf(b, n, "%.0f%%", v); return; }
        if (p == kAnaTilt) { snprintf(b, n, "%.1f dB/oct", v); return; }
        snprintf(b, n, "%.2f", v);
    }
    void fmt(uint32_t p, char* b, size_t n) const { if (p < kParamCount) fmtVal(p, val[p], b, n); else snprintf(b, n, "-"); }
    bool parse(const std::string& in, float& out) const {
        std::string s; for (char ch : in) if (!std::isspace((unsigned char)ch)) s += (char)std::tolower((unsigned char)ch);
        if (s.empty()) return false;
        char* end = nullptr; double x = std::strtod(s.c_str(), &end);
        if (end == s.c_str()) return false;
        std::string suf(end);
        if (suf == "k" || suf == "khz") x *= 1000;
        out = (float)x; return true;
    }
    uint32_t effParam(const Ctl& c) const {
        if (c.group == GBand || c.group == GDyn) {
            if (primary() < 0) return kParamCount;
            if (c.field == kFieldSlope) return slopeParam(val, primary());
            if (c.field >= 0) return bp(primary(), c.field);
            if (c.field2 >= 0) return bp2(primary(), c.field2);
            return kParamCount;
        }
        return c.param;
    }
    bool visible(const Ctl& c) const {
        switch (c.group) {
        case GBand: return primary() >= 0;
        case GDyn: return primary() >= 0 && canDynamic(shapeOf(primary()));
        case GAna: return anaMenu;
        default: return true;
        }
    }
    bool dimmed(const Ctl& c) const {
        if (c.param == kLinQuality) return (int)(val[kProcMode] + 0.5f) != kLinearPhase;
        if (c.group != GBand && c.group != GDyn) return false;
        const int b = primary(); if (b < 0) return false;
        const int sh = shapeOf(b);
        if (c.type == BandToggle || c.type == DelButton) return false;
        if (val[bp(b, kBOn)] < 0.5f && c.type != Hold) return true;
        if (c.field == kBGain) return !eq::hasGain(sh);
        if (c.field == kBQ) return !eq::hasQ(sh);
        if (c.field == kFieldSlope) return !eq::hasSlope(sh);
        if (c.field2 == kB2Dyn) return !canDynamic(sh);
        if (c.group == GDyn) {
            if (!dynOn(b)) return true;
            if (c.field2 == kB2ScFreq || c.field2 == kB2ScQ) return val[bp2(b, kB2ScFree)] < 0.5f || val[bp2(b, kB2Spectral)] > 0.5f;
            if (c.field2 == kB2ScFree) return val[bp2(b, kB2Spectral)] > 0.5f;
        }
        return false;
    }

    // ---- display mapping ------------------------------------------------------------
    float range() const { return rangeDb(val[kRange]); }
    float anaRangeDb() const { static const float t[3] = { 60.f, 90.f, 120.f }; return t[clampi((int)(val[kAnaRange] + 0.5f), 0, 2)]; }
    static float xOfHz(float hz) { return PL + PW * std::log(hz / FMIN) / std::log(FMAX / FMIN); }
    static float hzOfX(float x) { return FMIN * std::pow(FMAX / FMIN, (x - PL) / PW); }
    static float colHz(int i) { return hzOfX(PL + PW * (float)i / COLS); }
    float plotBottom() const { return DY + DH - 18 - (val[kPiano] > 0.5f ? 18.f : 0.f); }
    float yOfDb(float db) const { const float mid = DY + (plotBottom() - DY) / 2 + 4; return mid - db / range() * ((plotBottom() - DY) / 2 - 22); }
    float dbOfY(float y) const { const float mid = DY + (plotBottom() - DY) / 2 + 4; return (mid - y) / ((plotBottom() - DY) / 2 - 22) * range(); }
    float yOfAna(float db) const { return clampf(DY + 30 + (plotBottom() - DY - 34) * (-db / anaRangeDb()), DY + 30, plotBottom()); }
    // gainless shapes: the dot's height shows the resonance (Q relative to 0.707)
    float dotDb(int b) const {
        const int sh = shapeOf(b);
        return eq::hasGain(sh) ? val[bp(b, kBGain)] * gainScaleOf(val) : 20.f * std::log10(val[bp(b, kBQ)] / 0.70710678f);
    }
    void dotPos(int b, float& x, float& y) const { x = xOfHz(val[bp(b, kBFreq)]); y = yOfDb(clampf(dotDb(b), -range(), range())); }
    bool rangeHandlePos(int b, float& x, float& y) const {
        if (!dynOn(b)) return false;
        x = xOfHz(val[bp(b, kBFreq)]);
        y = yOfDb(clampf((val[bp(b, kBGain)] + val[bp2(b, kB2Range)]) * gainScaleOf(val), -range(), range()));
        return true;
    }

    // per-frame band designs for drawing
    void designAll() {
        const float fs = sampleRateUI;
        for (int b = 0; b < kNumBands; ++b) {
            designOk[b] = used(b);
            if (!designOk[b]) continue;
            eq::design(bandSpec(val, b, val[bp(b, kBGain)] * gainScaleOf(val)), fs, designs[b]);
            if (dynOn(b)) {
                eq::design(bandSpec(val, b, (val[bp(b, kBGain)] + val[bp2(b, kB2Range)]) * gainScaleOf(val)), fs, designsRange[b]);
                eq::design(bandSpec(val, b, val[bp(b, kBGain)] * gainScaleOf(val) + dynNow[b]), fs, designsNow[b]);
            }
        }
    }
    static float dbOf(cpx h) { return 20.f * std::log10(std::max(std::abs(h), 1e-6f)); }
    float totalDb(float hz, int c, bool live) const {
        cpx h(1.f, 0.f);
        for (int b = 0; b < kNumBands; ++b) {
            if (!designOk[b] || val[bp(b, kBOn)] < 0.5f) continue;
            if (!placeApplies((int)(val[bp(b, kBPlace)] + 0.5f), c)) continue;
            h *= eq::response(live && dynOn(b) ? designsNow[b] : designs[b], hz);
        }
        return dbOf(h);
    }

    // ---- drawing ------------------------------------------------------------
    void onNanoDisplay() override {
        const float s = uiScale();
        save(); scale(s, s);
        beginPath(); rect(0, 0, W, H); fillColor(C_BG); fill();
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        designAll();
        // logo: stacked translucent sheets
        for (int i = 0; i < 4; ++i) {
            beginPath(); const float o = i * 4.f;
            moveTo(16 + o, 26 + o); lineTo(30 + o, 20 + o); lineTo(38 + o, 26 + o); lineTo(24 + o, 32 + o); closePath();
            fillColor(withAlpha(C_ACC, 0.25f + 0.2f * i)); fill();
        }
        fontSize(22); textLetterSpacing(9); fillColor(C_TEXT); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); text(58, 34, "MICA", nullptr);
        fontSize(10); textLetterSpacing(2.5f); fillColor(C_DIM); text(162, 30, "EQUALIZER", nullptr);
        fontSize(7.5f); textLetterSpacing(1.8f); fillColor(C_FAINT); text(162, 42, "EPSILON AUDIO  2.1", nullptr);
        drawPresetBar();
        for (int k = 0; k < 2; ++k) {
            const float x = A_X + k * AB_W; const bool on = history.slot() == k, hot = hoverHeader == 6 + k;
            beginPath(); roundedRect(x, PS_Y, AB_W - 0.5f, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.85f) : (hot ? C_PANEL_HI : C_PANEL)); fill(); strokeColor(on ? C_ACC : C_LINE); strokeWidth(1); stroke();
            fontSize(11); textLetterSpacing(0); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_BG : C_DIM); text(x + AB_W / 2, PS_Y + PS_H / 2 + 0.5f, k ? "B" : "A", nullptr);
        }
        hdrButton(COPY_X, COPY_W, "COPY", false, hoverHeader == 8, false);
        hdrButton(UNDO_X, UR_W, "UNDO", false, hoverHeader == 3, !history.canUndo());
        hdrButton(REDO_X, UR_W, "REDO", false, hoverHeader == 4, !history.canRedo());
        hdrButton(LEARN_X, LEARN_W, learnMode ? "LEARNING" : "LEARN", learnMode, hoverHeader == 1, false);
        hdrButton(MIDI_X, MIDI_W, "MIDI", val[kMidiOn] > 0.5f, hoverHeader == 2, false);
        hdrButton(HELP_X, HELP_W, "HELP", help.open, hoverHeader == 5, false);
        {   // processing mode and latency, top right
            char b[64]; const int md = (int)(val[kProcMode] + 0.5f);
            const double sr = sampleRateUI;
            const double lat = latencySamples();
            if (lat > 0) snprintf(b, sizeof b, "%s  ·  %.1f ms", md == kLinearPhase ? "LINEAR PHASE" : (md == kNaturalPhase ? "NATURAL PHASE" : "ZERO LATENCY"), lat * 1000.0 / sr);
            else snprintf(b, sizeof b, "ZERO LATENCY");
            fontSize(8); textLetterSpacing(1.5f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(C_DIM); text(W - 16, 34, b, nullptr);
        }

        drawDisplay();
        drawMeter();
        panel(BX, PY, BW, PH); panel(GX, PY, GW, PH);
        caption(GX + 146, PY + 17, "PROCESSING"); caption(GX + 345, PY + 17, "QUALITY");
        caption(GX + 94, PY + 69, "CHANNELS"); caption(GX + 293, PY + 69, "CHARACTER");
        caption(GX + 94, PY + 121, "ANALYZER"); caption(GX + 252, PY + 121, "DISPLAY RANGE");
        const int pb = primary();
        if (pb < 0) {
            fontSize(12); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_DIM);
            text(BX + BW / 2, PY + PH / 2 - 10, used0() ? "Click a band's dot to edit it" : "Double-click the display, or click on the curve, to add a band", nullptr);
            fontSize(10); fillColor(C_FAINT); text(BX + BW / 2, PY + PH / 2 + 14, "Up to 24 bands  ·  Shift-click or drag a box to select several  ·  SKETCH to draw a curve", nullptr);
        } else {
            beginPath(); moveTo(BX + 318, PY + 86); lineTo(BX + 318, PY + PH - 12); strokeColor(C_LINE); strokeWidth(1); stroke();
            if (!canDynamic(shapeOf(pb))) {
                fontSize(10); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_FAINT);
                text(BX + 520, PY + 150, "Dynamics work on bells, shelves and tilts", nullptr);
            }
        }
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) if (visible(cs[i]) && cs[i].group != GAna) drawCtl(cs[i], i == active || i == hover);
        if (anaMenu) drawAnaMenu();
        if (matchOpen) drawMatch();
        if (editing >= 0) drawEditBox();
        if (menuOpen) drawPresetMenu();
        if (saving) drawSaveBox();
        if (help.open) help.draw(*this, theme, W, H, gTipsOn);
        else if (tips.shown && gTipsOn) { std::string t, v, bdy; if (tipContent(t, v, bdy)) epsilon::drawTooltip(*this, theme, tips.x, tips.y, W, H, t, v, bdy); }
        restore();
    }
    // what the engine reports (same rules as Engine::wantedLatency)
    double latencySamples() const {
        const int md = (int)(val[kProcMode] + 0.5f);
        const double sr = sampleRateUI;
        const int m = sr <= 50000.0 ? 1 : (sr <= 100000.0 ? 2 : 4);
        double l = md == kLinearPhase ? (2048 << clampi((int)(val[kLinQuality] + .5f), 0, 2)) * 1.5 : (md == kNaturalPhase ? 192.0 * m : 0.0);
        for (int b = 0; b < kNumBands; ++b) if (bandActive(val, b) && bandSpectral(val, b)) { l += 1024.0 * m; break; }
        return l;
    }
    bool used0() const { for (int b = 0; b < kNumBands; ++b) if (used(b)) return true; return false; }
    void panel(float x, float y, float w, float h) { beginPath(); roundedRect(x, y, w, h, 6); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke(); }
    void caption(float cx, float y, const char* t, int align = ALIGN_CENTER) { fontSize(7.5f); textLetterSpacing(1.8f); textAlign(align | ALIGN_MIDDLE); fillColor(C_FAINT); text(cx, y, t, nullptr); }
    void hdrButton(float x, float w, const char* label, bool on, bool hot, bool disabled) { epsilon::drawHeaderButton(*this, theme, x, PS_Y, w, PS_H, label, on, hot, disabled); }
    void toolButton(float x, float w, const char* label, bool on, bool hot) {
        beginPath(); roundedRect(x, TB_Y, w, TB_H, 4); fillColor(on ? withAlpha(C_ACC, 0.25f) : (hot ? C_PANEL_HI : C_PANEL)); fill();
        strokeColor(on ? C_ACC : C_LINE); strokeWidth(1); stroke();
        fontSize(8.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on || hot ? C_TEXT : C_DIM); text(x + w / 2, TB_Y + TB_H / 2 + 0.5f, label, nullptr);
    }

    void drawPiano() {
        const float y0 = plotBottom() + 1, h = 16;
        scissor(PL, y0, PW, h);
        for (int pass = 0; pass < 2; ++pass)
            for (int n = 12; n < 136; ++n) {
                const int k = n % 12; const bool black = k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
                if ((pass == 0) == black) continue;
                const float f = 440.f * std::exp2((n - 69) / 12.f);
                const float x0 = xOfHz(f * std::exp2(-1.f / 24.f)), x1 = xOfHz(f * std::exp2(1.f / 24.f));
                beginPath(); rect(x0, y0, std::max(0.5f, x1 - x0 - 0.5f), black ? h * 0.55f : h);
                fillColor(black ? Color(0x14, 0x16, 0x18) : (k == 0 ? Color(0xd8, 0xdc, 0xde) : Color(0xa8, 0xad, 0xb0))); fill();
            }
        for (int n = 24; n < 136; n += 12) {      // octave labels on the C keys, below the black keys
            const float f = 440.f * std::exp2((n - 69) / 12.f); char t[6]; snprintf(t, sizeof t, "C%d", n / 12 - 1);
            fontSize(6.5f); textLetterSpacing(0); textAlign(ALIGN_LEFT | ALIGN_BOTTOM); fillColor(Color(0x20, 0x24, 0x28)); text(xOfHz(f * std::exp2(-1.f / 24.f)) + 1, y0 + h, t, nullptr);
        }
        resetScissor();
    }

    void drawDisplay() {
        beginPath(); roundedRect(DX, DY, DW, DH, 6); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        const float yb = plotBottom();
        // grid
        const float grid[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        const char* gl[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
        beginPath();
        for (float g : grid) { const float x = xOfHz(g); moveTo(x, DY + 30); lineTo(x, yb); }
        const float r = range(), step = r <= 3 ? 1.f : (r <= 6 ? 2.f : (r <= 12 ? 3.f : 10.f));
        for (float db = -r; db <= r + 0.01f; db += step) { const float y = yOfDb(db); moveTo(DX + 26, y); lineTo(DX + DW - 8, y); }
        strokeColor(Color(255, 255, 255, 8 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(DX + 26, yOfDb(0)); lineTo(DX + DW - 8, yOfDb(0)); strokeColor(Color(255, 255, 255, 22 / 255.f)); stroke();
        fontSize(8.5f); textLetterSpacing(0.8f); fillColor(C_FAINT); textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        for (int i = 0; i < 10; ++i) text(xOfHz(grid[i]), DY + DH - 4, gl[i], nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        for (float db = -r; db <= r + 0.01f; db += step) { char b[8]; snprintf(b, sizeof b, "%+.0f", db); text(DX + 23, yOfDb(db), std::fabs(db) < 0.01f ? "0" : b, nullptr); }
        if (val[kPiano] > 0.5f) drawPiano();

        scissor(DX + 1, DY + 1, DW - 2, yb - DY);
        drawAnalyser();
        drawSpectralGain();
        // individual band curves (selected ones filled), dynamic ranges shaded
        for (int b = 0; b < kNumBands; ++b) {
            if (!designOk[b]) continue;
            const bool isSel = sel[b] || b == focus, on = val[bp(b, kBOn)] > 0.5f;
            const Color c = bandColor(b);
            if (dynOn(b) && on) {
                beginPath();
                for (int i = 0; i <= COLS; ++i) { const float f = colHz(i), x = PL + PW * i / COLS; const float y = clampf(yOfDb(dbOf(eq::response(designs[b], f))), DY - 40, DY + DH + 40); if (!i) moveTo(x, y); else lineTo(x, y); }
                for (int i = COLS; i >= 0; --i) { const float f = colHz(i), x = PL + PW * i / COLS; lineTo(x, clampf(yOfDb(dbOf(eq::response(designsRange[b], f))), DY - 40, DY + DH + 40)); }
                closePath(); fillColor(withAlpha(c, isSel ? 0.16f : 0.08f)); fill();
                if (std::fabs(dynNow[b]) > 0.05f) {
                    beginPath();
                    for (int i = 0; i <= COLS; ++i) { const float f = colHz(i), x = PL + PW * i / COLS; const float y = clampf(yOfDb(dbOf(eq::response(designsNow[b], f))), DY - 40, DY + DH + 40); if (!i) moveTo(x, y); else lineTo(x, y); }
                    strokeColor(withAlpha(c, 0.95f)); strokeWidth(1.6f); stroke();
                }
            }
            beginPath();
            for (int i = 0; i <= COLS; ++i) { const float f = colHz(i), x = PL + PW * i / COLS; const float y = clampf(yOfDb(dbOf(eq::response(designs[b], f))), DY - 40, DY + DH + 40); if (!i) moveTo(x, y); else lineTo(x, y); }
            if (isSel) {
                lineTo(PL + PW, yOfDb(0)); lineTo(PL, yOfDb(0)); closePath();
                fillColor(withAlpha(c, on ? 0.14f : 0.05f)); fill();
                beginPath();
                for (int i = 0; i <= COLS; ++i) { const float f = colHz(i), x = PL + PW * i / COLS; const float y = clampf(yOfDb(dbOf(eq::response(designs[b], f))), DY - 40, DY + DH + 40); if (!i) moveTo(x, y); else lineTo(x, y); }
            }
            strokeColor(withAlpha(c, on ? (isSel ? 0.9f : 0.35f) : 0.15f)); strokeWidth(isSel ? 1.5f : 1.f); stroke();
        }
        // combined curve(s): one line, or two when left/mid and right/side differ; live (with dynamics)
        bool split = false, anyDyn = false;
        for (int b = 0; b < kNumBands; ++b) if (used(b) && val[bp(b, kBOn)] > 0.5f) { if (val[bp(b, kBPlace)] > 0.5f) split = true; if (dynOn(b)) anyDyn = true; }
        for (int c = split ? 1 : 0; c >= 0; --c) {
            if (anyDyn) {   // the static curve, faint
                beginPath();
                for (int i = 0; i <= COLS; ++i) { const float x = PL + PW * i / COLS, y = clampf(yOfDb(totalDb(colHz(i), c, false)), DY - 40, DY + DH + 40); if (!i) moveTo(x, y); else lineTo(x, y); }
                strokeColor(withAlpha(C_TEXT, 0.3f)); strokeWidth(1.2f); stroke();
            }
            beginPath();
            for (int i = 0; i <= COLS; ++i) { const float x = PL + PW * i / COLS, y = clampf(yOfDb(totalDb(colHz(i), c, true)), DY - 40, DY + DH + 40); if (!i) moveTo(x, y); else lineTo(x, y); }
            strokeColor(split ? (c == 0 ? C_LEFT : C_RIGHT) : C_TEXT); strokeWidth(2.2f); stroke();
        }
        // sketch stroke
        if (sketching) {
            beginPath(); bool pen = false;
            for (int i = 0; i <= COLS; ++i) {
                if (std::isnan(sketchDb[i])) { pen = false; continue; }
                const float x = PL + PW * i / COLS, y = yOfDb(sketchDb[i]);
                if (!pen) moveTo(x, y); else lineTo(x, y); pen = true;
            }
            strokeColor(Color(0xff, 0xe0, 0x80)); strokeWidth(2.5f); stroke();
        }
        resetScissor();
        if (split) {
            const bool ms = val[kChannelMode] > 0.5f;
            fontSize(8.5f); textLetterSpacing(1.4f); textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
            fillColor(C_LEFT); text(DX + DW - 60, yb - 4, ms ? "MID" : "LEFT", nullptr);
            fillColor(C_RIGHT); text(DX + DW - 14, yb - 4, ms ? "SIDE" : "RIGHT", nullptr);
        }
        // spectrum grab marker
        if (grabPeak >= 0 && grabPeak < (int)peaks.size() && dragDot < 0) {
            const Peak& pk = peaks[grabPeak];
            const float x = xOfHz(pk.hz), y = yOfAna(pk.db);
            beginPath(); circle(x, y, 6); strokeColor(Color(0xff, 0xe0, 0x80)); strokeWidth(1.5f); stroke();
            beginPath(); moveTo(x, y + 8); lineTo(x, yOfDb(0)); strokeColor(withAlpha(Color(0xff, 0xe0, 0x80), 0.4f)); strokeWidth(1); stroke();
            char f[24], nb[24], line[64];
            if (pk.hz >= 1000) snprintf(f, sizeof f, "%.2f kHz", pk.hz / 1000); else snprintf(f, sizeof f, "%.0f Hz", pk.hz);
            if (val[kPiano] > 0.5f) snprintf(line, sizeof line, "%s  %s", f, noteName(pk.hz, nb, sizeof nb)); else snprintf(line, sizeof line, "%s", f);
            fontSize(9.5f); textLetterSpacing(0.4f); textAlign(ALIGN_CENTER | ALIGN_BOTTOM); fillColor(Color(0xff, 0xe0, 0x80)); text(x, y - 10, line, nullptr);
        }
        // dots and dynamic-range handles
        for (int b = 0; b < kNumBands; ++b) {
            if (!used(b)) continue;
            float x, y; dotPos(b, x, y);
            const bool isSel = sel[b] || b == focus, on = val[bp(b, kBOn)] > 0.5f, hot = hoverDot == b;
            const Color c = bandColor(b);
            float hx, hy;
            if (rangeHandlePos(b, hx, hy) && (isSel || hot || hoverHandle == b || dragHandle == b)) {
                beginPath(); moveTo(x, y); lineTo(hx, hy); strokeColor(withAlpha(c, 0.5f)); strokeWidth(1); stroke();
                beginPath(); moveTo(hx, hy - 6); lineTo(hx + 6, hy); lineTo(hx, hy + 6); lineTo(hx - 6, hy); closePath();
                fillColor(hoverHandle == b || dragHandle == b ? C_TEXT : withAlpha(c, 0.85f)); fill();
            }
            if (isSel || hot) { beginPath(); circle(x, y, 16); fillColor(withAlpha(c, 0.18f)); fill(); }
            if (dynOn(b) && on) { beginPath(); circle(x, y, 13); strokeColor(withAlpha(c, 0.8f)); strokeWidth(1.5f); stroke(); }
            beginPath(); circle(x, y, 10); fillColor(on ? c : C_PANEL_HI); fill();
            if (isSel) { strokeColor(C_TEXT); strokeWidth(2); stroke(); } else if (!on) { strokeColor(c); strokeWidth(1.2f); stroke(); }
            char t[4]; snprintf(t, sizeof t, "%d", b + 1);
            fontSize(9.5f); textLetterSpacing(0); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_BG : c); text(x, y + 0.5f, t, nullptr);
        }
        if (boxSel) {
            beginPath(); rect(std::min(boxX0, boxX1), std::min(boxY0, boxY1), std::fabs(boxX1 - boxX0), std::fabs(boxY1 - boxY0));
            fillColor(withAlpha(C_ACC, 0.08f)); fill(); strokeColor(withAlpha(C_ACC, 0.6f)); strokeWidth(1); stroke();
        }
        drawReadout();
        // toolbar
        toolButton(SK_X, SK_W, "SKETCH", sketchMode, hoverTool == 1);
        toolButton(MA_X, MA_W, "MATCH", matchOpen, hoverTool == 2);
        toolButton(AN_X, AN_W, "ANALYZER  \xe2\x96\xbe", anaMenu, hoverTool == 3);
        if (val[kAnaFreeze] > 0.5f) { fontSize(8.5f); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_EXT); text(AN_X + AN_W + 10, TB_Y + TB_H / 2, "FROZEN", nullptr); }
        drawStatus();
        if (learnMode) drawActiveChips();
    }
    void drawReadout() {
        const int rb = dragDot >= 0 ? dragDot : (dragHandle >= 0 ? dragHandle : hoverDot);
        if (rb < 0 || !used(rb)) return;
        float x, y; dotPos(rb, x, y);
        char f[24], g[24], q[24], nb[24], line[160];
        fmt(bp(rb, kBFreq), f, sizeof f); fmt(bp(rb, kBGain), g, sizeof g); fmt(bp(rb, kBQ), q, sizeof q);
        const int sh = shapeOf(rb);
        std::string s = std::to_string(rb + 1) + "  " + eq::shapeName(sh) + "   " + f;
        if (val[kPiano] > 0.5f) { s += "  "; s += noteName(val[bp(rb, kBFreq)], nb, sizeof nb); }
        if (eq::hasGain(sh)) { s += "   "; s += g; }
        if (eq::hasQ(sh)) { s += "   Q "; s += q; }
        if (dynOn(rb)) { char r[48]; snprintf(r, sizeof r, "   DYN %+.1f dB (now %+.1f)", val[bp2(rb, kB2Range)], dynNow[rb]); s += r; }
        snprintf(line, sizeof line, "%s", s.c_str());
        fontSize(10); textLetterSpacing(0.4f);
        Rectangle<float> tb; textBounds(0, 0, line, nullptr, tb);
        const float w = tb.getWidth() + 16, bx = clampf(x - w / 2, DX + 6, DX + DW - w - 6), by = y > DY + 80 ? y - 42 : y + 20;
        beginPath(); roundedRect(bx, by, w, 20, 4); fillColor(Color(0x1d, 0x21, 0x25, 0.95f)); fill(); strokeColor(withAlpha(bandColor(rb), 0.8f)); strokeWidth(1); stroke();
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(bx + 8, by + 10.5f, line, nullptr);
    }
    void drawAnalyser() {
        const int mode = (int)(val[kAnalyzer] + 0.5f);
        const bool ext = val[kAnaExternal] > 0.5f;
        if (!anaValid || (mode == 0 && !ext)) return;
        const float yb = plotBottom();
        auto pathOf = [&](int s) { beginPath(); moveTo(PL, yb); for (int i = 0; i <= COLS; ++i) lineTo(PL + PW * i / COLS, yOfAna(col[s][i])); lineTo(PL + PW, yb); closePath(); };
        auto lineOf = [&](int s) { beginPath(); for (int i = 0; i <= COLS; ++i) { const float x = PL + PW * i / COLS, y = yOfAna(col[s][i]); if (!i) moveTo(x, y); else lineTo(x, y); } };
        if (mode == 1 || mode == 3) { pathOf(0); fillPaint(linearGradient(0, DY, 0, yb, withAlpha(C_ANA, mode == 3 ? 0.22f : 0.45f), withAlpha(C_ANA, 0.05f))); fill(); }
        if (mode == 2 || mode == 3) {
            pathOf(1); fillPaint(linearGradient(0, DY, 0, yb, withAlpha(C_ANA, 0.4f), withAlpha(C_ANA, 0.06f))); fill();
            if (mode == 3) { lineOf(1); strokeColor(withAlpha(Color(0x9a, 0xc0, 0xd0), 0.7f)); strokeWidth(1); stroke(); }
        }
        if (ext) {
            lineOf(2); strokeColor(withAlpha(C_EXT, 0.85f)); strokeWidth(1.2f); stroke();
            if (val[kCollision] > 0.5f) {   // red where both are loud and close in level
                const int main = mode == 1 ? 0 : 1;
                for (int i = 0; i < COLS; ++i) {
                    const float a = col[main][i], b = col[2][i];
                    if (a < -anaRangeDb() * 0.55f || b < -anaRangeDb() * 0.55f || std::fabs(a - b) > 9.f) continue;
                    const float x0 = PL + PW * i / COLS, x1 = PL + PW * (i + 1) / COLS;
                    const float strength = 1.f - std::fabs(a - b) / 9.f;
                    beginPath(); rect(x0, yOfAna(std::max(a, b)), x1 - x0 + 0.5f, yb - yOfAna(std::max(a, b)));
                    fillColor(withAlpha(C_HOT, 0.12f + 0.3f * strength)); fill();
                }
            }
        }
    }
    void drawSpectralGain() {
        bool any = false; for (int b = 0; b < kNumBands; ++b) if (bandActive(val, b) && bandSpectral(val, b)) any = true;
        if (!any || specGain.empty()) return;
        const float y0 = yOfDb(0);
        beginPath(); moveTo(PL, y0);
        const int K = (int)specGain.size();
        for (int i = 0; i <= COLS; ++i) {
            const float pos = colHz(i) / specBinHz; const int k = clampi((int)pos, 0, K - 2); const float t = clampf(pos - k, 0.f, 1.f);
            lineTo(PL + PW * i / COLS, yOfDb(clampf(specGain[k] * (1 - t) + specGain[k + 1] * t, -range(), range())));
        }
        lineTo(PL + PW, y0); closePath();
        fillColor(withAlpha(Color(0xff, 0xe0, 0x80), 0.25f)); fill();
    }
    void drawMeter() {
        beginPath(); roundedRect(MX, DY, MW, DH, 6); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        const float top = DY + 26, bot = DY + DH - 22;
        auto yOf = [&](float db) { return top + (bot - top) * clampf(-db / 60.f, 0.f, 1.f); };
        for (int c = 0; c < 2; ++c) {
            const float x = MX + 7 + c * 12, w = 9;
            beginPath(); rect(x, top, w, bot - top); fillColor(C_PANEL); fill();
            const float y = yOf(meterDb[c]);
            beginPath(); rect(x, y, w, bot - y);
            fillPaint(linearGradient(0, top, 0, bot, meterDb[c] > -0.1f ? C_HOT : Color(0xe8, 0xd8, 0x80), C_ACC)); fill();
        }
        fontSize(7.5f); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(C_FAINT);
        for (float db : { 0.f, -12.f, -24.f, -36.f, -48.f }) { beginPath(); moveTo(MX + 4, yOf(db)); lineTo(MX + MW - 4, yOf(db)); strokeColor(withAlpha(C_LINE, 0.6f)); stroke(); }
        char b[12]; if (meterHold < -99) snprintf(b, sizeof b, "-inf"); else snprintf(b, sizeof b, "%.1f", meterHold);
        fillColor(meterHold > -0.1f ? C_HOT : C_DIM); text(MX + MW / 2, DY + 12, b, nullptr);
        fillColor(C_FAINT); text(MX + MW / 2, DY + DH - 10, "OUT", nullptr);
        if (val[kAutoGain] > 0.5f) { char a[16]; snprintf(a, sizeof a, "%+.1f", autoGainUI); fillColor(C_ACC); text(MX + MW / 2, DY + DH - 30, a, nullptr); }
    }
    void drawActiveChips() {
        static const char* const lab[3] = { "FREQ", "GAIN", "Q" };
        const float y = TB_Y + 1;
        fontSize(8); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_ACC); text(CH_X - 92, y + 9, "ACTIVE BAND", nullptr);
        for (int i = 0; i < 3; ++i) {
            const float x = CH_X + i * 76; const bool waiting = learnWaiting == MicaPlugin::kActiveTarget + i, hot = hoverChip == i;
            beginPath(); roundedRect(x, y, 70, 18, 4); fillColor(waiting ? C_ACC : (hot ? C_PANEL_HI : C_PANEL)); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
            char t[24]; if (waiting) snprintf(t, sizeof t, "%s ?", lab[i]); else if (activeCC[i] >= 0) snprintf(t, sizeof t, "%s CC%d", lab[i], activeCC[i]); else snprintf(t, sizeof t, "%s", lab[i]);
            fontSize(8.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(waiting ? C_BG : C_TEXT); text(x + 35, y + 9.5f, t, nullptr);
        }
    }
    void drawAnaMenu() {
        beginPath(); rect(AX - 10, AY - 4, AW + 20, AH + 20); fillPaint(boxGradient(AX, AY + 2, AW, AH, 6, 14, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(AX, AY, AW, AH, 6); fillColor(C_PANEL_HI); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        for (float yy : { AY + 96.f, AY + 128.f, AY + 160.f, AY + 192.f }) {
            const char* n = yy < AY + 100 ? "RANGE" : (yy < AY + 130 ? "SPEED" : (yy < AY + 170 ? "RESOLUTION" : "TILT"));
            caption(AX + 16, yy, n, ALIGN_LEFT);
        }
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) if (cs[i].group == GAna) drawCtl(cs[i], i == active || i == hover);
    }
    void drawMatch() {
        beginPath(); rect(MPX - 10, MPY - 4, MPW + 20, MPH + 20); fillPaint(boxGradient(MPX, MPY + 2, MPW, MPH, 6, 14, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(MPX, MPY, MPW, MPH, 6); fillColor(C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(9.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_ACC); text(MPX + 16, MPY + 12, "EQ MATCH", nullptr);
        char b[160];
        if (matchListening) snprintf(b, sizeof b, "Listening: %.1f s captured. Play your track and the reference (in the sidechain) together.", matchSecs);
        else if (matchFrames > 0) snprintf(b, sizeof b, "%.1f s captured. APPLY builds bands from it (they replace the current ones); LISTEN adds more.", matchSecs);
        else snprintf(b, sizeof b, "Route a reference track to Mica's sidechain, click LISTEN and play both for 10 seconds or more.");
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f); fillColor(C_TEXT); textBox(MPX + 16, MPY + 34, MPW - 32, b, nullptr); textLineHeight(1.f);
        if (!matchMsg.empty()) { fontSize(9.5f); fillColor(C_EXT); textAlign(ALIGN_LEFT | ALIGN_TOP); text(MPX + 16, MPY + 70, matchMsg.c_str(), nullptr); }
        const char* labels[3] = { matchListening ? "STOP" : "LISTEN", "APPLY", "CLOSE" };
        for (int i = 0; i < 3; ++i) {
            const float x = MPX + MPW - 16 - (3 - i) * 92 + 6, y = MPY + MPH - 36; const bool hot = hoverMatch == i;
            const bool dis = i == 1 && matchFrames == 0;
            beginPath(); roundedRect(x, y, 86, 24, 5); fillColor(i == 0 && matchListening ? withAlpha(C_ACC, 0.4f) : (hot && !dis ? withAlpha(C_ACC, 0.25f) : C_PANEL)); fill(); strokeColor(dis ? C_LINE : C_ACC); strokeWidth(1); stroke();
            fontSize(9); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(dis ? C_FAINT : C_TEXT); text(x + 43, y + 12.5f, labels[i], nullptr);
        }
    }
    int matchHit(float x, float y) const {
        if (!matchOpen) return -2;
        for (int i = 0; i < 3; ++i) { const float bx = MPX + MPW - 16 - (3 - i) * 92 + 6, by = MPY + MPH - 36; if (x >= bx && x < bx + 86 && y >= by && y < by + 24) return i; }
        return inRect(x, y, MPX, MPY, MPW, MPH) ? -1 : -2;
    }

    void drawKnobBody(float x, float y, float r, float n, bool bipolar, bool hot, Color c, bool dim) {
        const float a0 = 0.75f * 3.14159265f, a1 = 2.25f * 3.14159265f, ang = a0 + (a1 - a0) * n;
        beginPath(); arc(x, y, r, a0, a1, NanoVG::CW); strokeColor(C_LINE); strokeWidth(r > 24 ? 5.f : 3.5f); lineCap(ROUND); stroke();
        const float from = bipolar ? (a0 + a1) / 2 : a0;
        if (std::fabs(ang - from) > 0.001f) { beginPath(); arc(x, y, r, std::min(from, ang), std::max(from, ang), NanoVG::CW); strokeColor(dim ? C_FAINT : c); stroke(); }
        const float cr = r - (r > 24 ? 9 : 6);
        beginPath(); circle(x, y, cr);
        fillPaint(radialGradient(x - cr * 0.3f, y - cr * 0.4f, 0, cr * 1.4f, hot ? Color(0x3a, 0x40, 0x46) : C_PANEL_HI, Color(0x13, 0x15, 0x17)));
        fill(); strokeColor(Color(255, 255, 255, 18 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(x + std::cos(ang) * cr * 0.35f, y + std::sin(ang) * cr * 0.35f); lineTo(x + std::cos(ang) * cr * 0.9f, y + std::sin(ang) * cr * 0.9f);
        strokeColor(dim ? C_DIM : C_TEXT); strokeWidth(r > 24 ? 2.5f : 2.f); lineCap(ROUND); stroke();
    }
    // segment captions and the value each segment stands for
    const char* const* segNames(const Ctl& c, int& n, const int** map) const {
        static const char* const shape[] = { "BELL", "LO SHF", "HI SHF", "LO CUT", "HI CUT", "NOTCH", "B-PASS", "TILT", "FLAT T", "A-PASS" };
        static const char* const placeLR[] = { "STEREO", "LEFT", "RIGHT" };
        static const char* const placeMS[] = { "STEREO", "MID", "SIDE" };
        static const char* const mode[] = { "ZERO LAT", "NATURAL", "LINEAR" };
        static const int modeMap[] = { kZeroLatency, kNaturalPhase, kLinearPhase };
        static const char* const lq[] = { "LOW", "MED", "HIGH" };
        static const char* const chm[] = { "STEREO", "MID / SIDE" };
        static const char* const chr[] = { "CLEAN", "GENTLE", "WARM" };
        static const char* const ana[] = { "OFF", "PRE", "POST", "BOTH" };
        static const char* const rng[] = { "3", "6", "12", "30" };
        static const int rngMap[] = { 3, 0, 1, 2 };
        static const char* const arng[] = { "60 dB", "90 dB", "120 dB" };
        static const char* const spd[] = { "V.SLOW", "SLOW", "MED", "FAST", "V.FAST" };
        static const char* const res[] = { "LOW", "MED", "HIGH", "MAX" };
        static const char* const src[] = { "INTERNAL", "EXTERNAL" };
        *map = nullptr;
        if (c.field == kBShape) { n = 10; return shape; }
        if (c.field == kBPlace) { n = 3; return val[kChannelMode] > 0.5f ? placeMS : placeLR; }
        if (c.field2 == kB2Source) { n = 2; return src; }
        switch (c.param) {
        case kProcMode: n = 3; *map = modeMap; return mode;
        case kLinQuality: n = 3; return lq;
        case kChannelMode: n = 2; return chm;
        case kCharacter: n = 3; return chr;
        case kAnalyzer: n = 4; return ana;
        case kRange: n = 4; *map = rngMap; return rng;
        case kAnaRange: n = 3; return arng;
        case kAnaSpeed: n = 5; return spd;
        case kAnaRes: n = 4; return res;
        default: n = 0; return nullptr;
        }
    }
    void drawSwitch(float x, float y, bool on, const char* label, Color c, bool dim, bool hot) {
        const float w = 30, h = 16, x0 = x - w / 2, y0 = y - h / 2;
        beginPath(); roundedRect(x0, y0, w, h, h / 2); fillColor(on ? withAlpha(c, dim ? 0.12f : 0.3f) : C_PANEL_HI); fill(); strokeColor(on && !dim ? c : C_LINE); strokeWidth(1); stroke();
        beginPath(); circle(on ? x0 + w - h / 2 : x0 + h / 2, y, 5.5f); fillColor(dim ? C_FAINT : (on ? c : C_DIM)); fill();
        if (label && *label) { fontSize(9); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(dim ? C_FAINT : (on || hot ? C_TEXT : C_DIM)); text(x + w / 2 + 8, y + 0.5f, label, nullptr); }
    }
    void drawCtl(const Ctl& c, bool hot) {
        const uint32_t p = effParam(c); if (p >= kParamCount && c.type != DelButton) return;
        const bool dim = dimmed(c); char buf2[48];
        const Color col2 = (c.group == GBand || c.group == GDyn) && primary() >= 0 ? bandColor(primary()) : C_ACC;
        switch (c.type) {
        case Knob: {
            const int f2 = field2Of(p);
            const bool bip = fieldOf(p) == kBGain || p == kOutGain || f2 == kB2Range || p == kGainScale;
            float n = toNorm(p, val[p]);
            if (p == kGainScale) n = (val[p] + 100.f) / 300.f;
            drawKnobBody(c.x, c.y, c.r, n, bip && p != kGainScale, hot, col2, dim);
            if (p == kGainScale) {   // mark 100%
                const float a = 0.75f * 3.14159265f + 1.5f * 3.14159265f * (200.f / 300.f);
                beginPath(); moveTo(c.x + std::cos(a) * (c.r + 3), c.y + std::sin(a) * (c.r + 3)); lineTo(c.x + std::cos(a) * (c.r + 6), c.y + std::sin(a) * (c.r + 6)); strokeColor(C_DIM); strokeWidth(1.5f); stroke();
            }
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            const bool big = c.r >= 26;
            fontSize(big ? 8.5f : 7.5f); textLetterSpacing(big ? 1.6f : 1.2f); fillColor(dim ? C_FAINT : C_DIM); text(c.x, c.y + c.r + (big ? 6 : 4), c.label, nullptr);
            fmt(p, buf2, sizeof buf2); fontSize(big ? 11 : 9.5f); textLetterSpacing(0.3f); fillColor(dim ? C_FAINT : (hot ? col2 : C_TEXT)); text(c.x, c.y + c.r + (big ? 19 : 14), buf2, nullptr);
            break; }
        case BandToggle: {
            const bool on = val[p] > 0.5f;
            drawSwitch(c.x, c.y, on, "", col2, false, hot);
            const int ns = selCount(); char t[24];
            if (ns > 1) snprintf(t, sizeof t, "%d BANDS", ns); else snprintf(t, sizeof t, "BAND %d", primary() + 1);
            fontSize(11); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM); text(c.x + 24, c.y + (ns > 1 ? -4.f : 0.5f), t, nullptr);
            if (ns > 1) { fontSize(7); textLetterSpacing(1.4f); fillColor(C_ACC); text(c.x + 24, c.y + 9, "EDITS APPLY TO ALL", nullptr); }
            break; }
        case Switch: drawSwitch(c.x, c.y, val[p] > 0.5f, c.label, col2, dim, hot); break;
        case Segment: {
            int n = 0; const int* map = nullptr; const char* const* names = segNames(c, n, &map);
            const int cur = (int)(val[p] + .5f); const float sw = c.r / n, x0 = c.x - c.r / 2;
            for (int i = 0; i < n; ++i) {
                const float x = x0 + sw * i; const bool on = (map ? map[i] : i) == cur;
                beginPath(); roundedRect(x + 1.5f, c.y - SEG_H / 2, sw - 3, SEG_H, 5); fillColor(on ? (dim ? C_FAINT : col2) : C_PANEL); fill();
                if (!on && hot && hoverSeg == i) { strokeColor(C_DIM); strokeWidth(1); stroke(); }
                fontSize(n >= 8 ? 8.f : 8.5f); textLetterSpacing(n >= 8 ? 0.8f : 1.3f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_BG : (dim ? C_FAINT : C_DIM)); text(x + sw / 2, c.y + 0.5f, names[i], nullptr);
            }
            break; }
        case Stepper: {
            const float x0 = c.x - c.r / 2, y0 = c.y - SEG_H / 2;
            beginPath(); roundedRect(x0, y0, c.r, SEG_H, 5); fillColor(C_PANEL_HI); fill(); strokeColor(hot && !dim ? C_DIM : C_LINE); strokeWidth(1); stroke();
            for (int s = 0; s < 2; ++s) {
                const float cx = s == 0 ? x0 + 11 : x0 + c.r - 11, d = s == 0 ? -1.f : 1.f;
                beginPath(); moveTo(cx - 3 * d, c.y - 5); lineTo(cx + 3 * d, c.y); lineTo(cx - 3 * d, c.y + 5); closePath(); fillColor(dim ? C_FAINT : (hot && hoverSeg == s ? col2 : C_DIM)); fill();
            }
            fmt(p, buf2, sizeof buf2);
            if (c.group == GBand) { std::string t = std::string(c.label) + "  " + buf2; snprintf(buf2, sizeof buf2, "%s", t.c_str()); }
            fontSize(9); textLetterSpacing(1.f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(dim ? C_FAINT : C_TEXT); text(c.x, c.y + 0.5f, buf2, nullptr);
            break; }
        case Hold: {
            const bool on = soloing;
            beginPath(); roundedRect(c.x - c.r / 2, c.y - SEG_H / 2, c.r, SEG_H, 5); fillColor(on ? Color(0xe8, 0xc0, 0x50) : (hot ? C_PANEL_HI : C_PANEL)); fill(); strokeColor(on ? Color(0xe8, 0xc0, 0x50) : C_LINE); strokeWidth(1); stroke();
            fontSize(9); textLetterSpacing(1.8f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_BG : C_DIM); text(c.x, c.y + 0.5f, c.label, nullptr);
            break; }
        case DelButton: {
            beginPath(); roundedRect(c.x - 13, c.y - 11, 26, 22, 5); fillColor(hot ? withAlpha(C_HOT, 0.3f) : C_PANEL_HI); fill(); strokeColor(hot ? C_HOT : C_LINE); strokeWidth(1); stroke();
            beginPath(); moveTo(c.x - 5, c.y - 5); lineTo(c.x + 5, c.y + 5); moveTo(c.x + 5, c.y - 5); lineTo(c.x - 5, c.y + 5); strokeColor(hot ? C_TEXT : C_DIM); strokeWidth(1.6f); stroke();
            break; }
        }
        if (learnMode && c.type != DelButton && c.type != Hold && p < kParamCount) {
            const int cc = paramCC[p]; const bool waiting = learnWaiting == (int)p;
            if (cc >= 0 || waiting) {
                const float by = c.type == Knob ? c.y - c.r - 10 : c.y - SEG_H / 2 - 7;
                snprintf(buf2, sizeof buf2, waiting ? "?" : "CC%d", cc);
                fontSize(8.5f); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                const float bw = waiting ? 16 : 34;
                beginPath(); roundedRect(c.x - bw / 2, by - 7, bw, 14, 3); fillColor(waiting ? C_ACC : C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fillColor(waiting ? C_BG : C_ACC); text(c.x, by + 0.5f, buf2, nullptr);
            }
        }
    }
    void drawStatus() {
        std::string msg;
        if (flashMsg.active()) msg = flashMsg.text;
        else if (learnMode) {
            if (learnWaiting >= MicaPlugin::kActiveTarget) msg = "Move a MIDI controller to assign the active band's control";
            else if (learnWaiting >= 0) msg = std::string("Move a MIDI controller to assign: ") + paramDef(learnWaiting).name;
            else msg = "Click a control, then move a MIDI controller.  Right-click unassigns.";
        } else if (sketchMode) msg = "SKETCH: draw the curve you want; Mica places bands when you let go";
        if (!msg.empty()) { fontSize(10); textLetterSpacing(0.8f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(C_ACC); text(DX + DW - 12, TB_Y + TB_H / 2, msg.c_str(), nullptr); }
        if (learnMode && learnWaiting < 0) { fontSize(9); textAlign(ALIGN_RIGHT | ALIGN_BOTTOM); fillColor(hoverClear ? C_TEXT : C_DIM); text(DX + DW - 12, plotBottom() - 8, "CLEAR ALL ASSIGNMENTS", nullptr); }
    }
    void flash(const std::string& s) { flashMsg.show(s); repaint(); }
    void drawEditBox() {
        const Ctl& c = controls()[editing];
        const float w = 90, h = 22, x = c.x - w / 2, y = c.y + c.r + 4;
        beginPath(); roundedRect(x, y, w, h, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(12); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(c.x, y + h / 2 + 0.5f, (editBuf + "|").c_str(), nullptr);
    }

    // ---- presets ------------------------------------------------------------------
    bool matchesValues(const float* v) const {
        for (uint32_t i = 0; i < kParamCount; ++i) {
            if (!isSetting(i)) continue;
            if (std::fabs(val[i] - v[i]) > 1e-4f * std::max(1.f, std::fabs(v[i]))) return false;
        }
        return true;
    }
    void applyValues(const float* v) { beginGesture(); applyAll(v); endGesture(); for (bool& s : sel) s = false; focus = -1; }
    void applyFactory(int idx) { idx = (idx % (int)kNumPresets + (int)kNumPresets) % (int)kNumPresets; applyValues(presets()[idx].v); curFactory = idx; curUser.clear(); repaint(); }
    void applyUser(const std::string& name) {
        float v[kParamCount]; std::copy(defv, defv + kParamCount, v);
        if (!store.load(name, pparams, kParamCount, v)) return;
        applyValues(v); std::copy(v, v + kParamCount, userVals); curUser = name; curFactory = -1; repaint();
    }
    void programLoaded(uint32_t index) override { curFactory = (int)index; curUser.clear(); for (bool& s : sel) s = false; focus = -1; repaint(); }
    void step(int d) {
        const int nf = (int)kNumPresets, nu = (int)userPresets.size(), total = nf + nu;
        int cur = curFactory; if (!curUser.empty()) for (int i = 0; i < nu; ++i) if (userPresets[i] == curUser) cur = nf + i;
        if (cur < 0) cur = 0;
        const int next = ((cur + d) % total + total) % total;
        if (next < nf) applyFactory(next); else applyUser(userPresets[next - nf]);
    }
    void drawPresetBar() {
        const float x = PS_X, y = PS_Y, w = PS_W, h = PS_H, a = PS_ARROW;
        beginPath(); roundedRect(x, y, w, h, 5); fillColor(C_PANEL); fill(); strokeColor(menuOpen ? C_ACC : C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(x + a, y + 5); lineTo(x + a, y + h - 5); moveTo(x + w - a, y + 5); lineTo(x + w - a, y + h - 5); strokeColor(C_LINE); stroke();
        for (int side = 0; side < 2; ++side) {
            const bool hot = presetHover == (side == 0 ? 0 : 2);
            const float cx = side == 0 ? x + a / 2 : x + w - a / 2, cy = y + h / 2, d = side == 0 ? -1.f : 1.f;
            beginPath(); moveTo(cx - 3 * d, cy - 5); lineTo(cx + 3 * d, cy); lineTo(cx - 3 * d, cy + 5); closePath(); fillColor(hot ? C_ACC : C_DIM); fill();
        }
        std::string name; bool modified = false;
        if (!curUser.empty()) { name = curUser; modified = !matchesValues(userVals); }
        else if (curFactory >= 0) { name = presets()[curFactory].name; modified = !matchesValues(presets()[curFactory].v); }
        else name = "Custom";
        if (modified) name += " *";
        fontSize(12); textLetterSpacing(1.f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(presetHover == 1 || menuOpen ? C_TEXT : Color(0xd2, 0xd6, 0xd8));
        text(x + w / 2 - 6, y + h / 2 + 0.5f, name.c_str(), nullptr);
        const float cx = x + w - a - 12, cy = y + h / 2;
        beginPath(); moveTo(cx - 4, cy - 2); lineTo(cx + 4, cy - 2); lineTo(cx, cy + 3); closePath(); fillColor(C_DIM); fill();
    }
    // preset browser: categories, user presets and favourites in columns; typing searches names and categories.
    // kind: 0 factory, 1 user, 2 save button, 3 search field, 4 favourite (idx into favs)
    struct MRect { float x, y, w, h; int kind; int idx; };
    static std::string lower(const std::string& s) { std::string r(s); for (char& c : r) c = (char)std::tolower((unsigned char)c); return r; }
    std::string favKey(int kind, int idx) const { return kind == 0 ? std::string("f:") + presets()[idx].name : (kind == 1 ? "u:" + userPresets[idx] : (idx < (int)favs.size() ? favs[idx] : std::string())); }
    bool isFav(const std::string& k) const { return std::find(favs.begin(), favs.end(), k) != favs.end(); }
    void toggleFav(const std::string& k) {
        if (k.empty()) return;
        auto it = std::find(favs.begin(), favs.end(), k); if (it != favs.end()) favs.erase(it); else favs.push_back(k);
        store.setFavourites(favs); repaint();
    }
    static constexpr int kMenuCols = kNumCategories + 2, kMenuRows = 16;
    void menuLayout(float& mx, float& my, float& mw, float& mh, std::vector<MRect>& items) const {
        mw = PM_COLW * kMenuCols + 2 * PM_PAD; mx = std::min(W - mw - 8, std::max(8.f, PS_X + PS_W / 2 - mw / 2)); my = PS_Y + PS_H + 4;
        items.clear();
        const float top = my + PM_PAD + PM_ROW + 6 + PM_HEAD;
        int maxRows = 1;
        items.push_back({ mx + PM_PAD + PM_COLW, my + PM_PAD, 300, PM_ROW, 3, 0 });
        items.push_back({ mx + mw - PM_PAD - 150, my + PM_PAD, 150, PM_ROW, 2, 0 });
        if (!query.empty()) {
            const std::string q = lower(query); int k = 0;
            auto place = [&](int kind, int idx) { const int col = k / kMenuRows, row = k % kMenuRows; if (col >= kMenuCols) return; items.push_back({ mx + PM_PAD + col * PM_COLW, top + row * PM_ROW, PM_COLW - 4, PM_ROW, kind, idx }); maxRows = std::max(maxRows, row + 1); ++k; };
            for (uint32_t i = 0; i < kNumPresets; ++i) if (lower(presets()[i].name).find(q) != std::string::npos || lower(presets()[i].category).find(q) != std::string::npos) place(0, (int)i);
            for (int u = 0; u < (int)userPresets.size(); ++u) if (lower(userPresets[u]).find(q) != std::string::npos || std::string("user").find(q) != std::string::npos) place(1, u);
        } else {
            int rows[kNumCategories] = {};
            for (uint32_t i = 0; i < kNumPresets; ++i) {
                const char* cat = presets()[i].category;
                if (std::strcmp(cat, "Init") == 0) { items.push_back({ mx + PM_PAD, my + PM_PAD, PM_COLW - 4, PM_ROW, 0, (int)i }); continue; }
                for (int c = 0; c < kNumCategories; ++c)
                    if (std::strcmp(cat, kCategories[c]) == 0) { items.push_back({ mx + PM_PAD + c * PM_COLW, top + rows[c] * PM_ROW, PM_COLW - 4, PM_ROW, 0, (int)i }); maxRows = std::max(maxRows, ++rows[c]); }
            }
            for (int u = 0; u < (int)userPresets.size() && u < kMenuRows; ++u) items.push_back({ mx + PM_PAD + kNumCategories * PM_COLW, top + u * PM_ROW, PM_COLW - 4, PM_ROW, 1, u });
            maxRows = std::max(maxRows, std::min(kMenuRows, (int)userPresets.size()));
            for (int f = 0; f < (int)favs.size() && f < kMenuRows; ++f) items.push_back({ mx + PM_PAD + (kNumCategories + 1) * PM_COLW, top + f * PM_ROW, PM_COLW - 4, PM_ROW, 4, f });
            maxRows = std::max(maxRows, std::min(kMenuRows, (int)favs.size()));
        }
        mh = PM_PAD + PM_ROW + 6 + PM_HEAD + maxRows * PM_ROW + PM_PAD;
    }
    std::string itemLabel(const MRect& r) const {
        if (r.kind == 0) return presets()[r.idx].name;
        if (r.kind == 1) return userPresets[r.idx];
        if (r.kind == 4 && r.idx < (int)favs.size()) return favs[r.idx].substr(2);
        return "";
    }
    void drawPresetMenu() {
        float mx, my, mw, mh; std::vector<MRect> items; menuLayout(mx, my, mw, mh, items);
        beginPath(); rect(mx - 12, my - 4, mw + 24, mh + 20); fillPaint(boxGradient(mx, my + 2, mw, mh, 6, 14, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(mx, my, mw, mh, 6); fillColor(C_PANEL_HI); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(mx + PM_PAD, my + PM_PAD + PM_ROW + 3); lineTo(mx + mw - PM_PAD, my + PM_PAD + PM_ROW + 3); strokeColor(C_LINE); stroke();
        fontSize(8.5f); textLetterSpacing(1.8f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        const float hy = my + PM_PAD + PM_ROW + 6 + PM_HEAD / 2;
        if (query.empty()) {
            for (int c = 0; c < kMenuCols; ++c) {
                std::string up = c < kNumCategories ? kCategories[c] : (c == kNumCategories ? "User" : "Favourites"); for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
                fillColor(c < kNumCategories ? C_ACC : C_FAV); text(mx + PM_PAD + c * PM_COLW + 12, hy, up.c_str(), nullptr);
            }
            fontSize(10); textLetterSpacing(0.4f); fillColor(C_FAINT);
            if (userPresets.empty()) text(mx + PM_PAD + kNumCategories * PM_COLW + 12, hy + PM_HEAD / 2 + PM_ROW / 2, "(none saved yet)", nullptr);
            if (favs.empty()) text(mx + PM_PAD + (kNumCategories + 1) * PM_COLW + 12, hy + PM_HEAD / 2 + PM_ROW / 2, "(click a star)", nullptr);
        } else {
            int found = 0; for (const MRect& r : items) if (r.kind == 0 || r.kind == 1) ++found;
            char b[64]; snprintf(b, sizeof b, found ? "%d MATCHING PRESET%s" : "NO PRESET MATCHES", found, found == 1 ? "" : "S"); fillColor(C_ACC); text(mx + PM_PAD + 12, hy, b, nullptr);
        }
        for (int i = 0; i < (int)items.size(); ++i) {
            const MRect& r = items[i]; const bool hot = i == menuHover;
            if (r.kind == 2) {
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(hot ? withAlpha(C_ACC, 0.25f) : C_PANEL); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fontSize(9.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(r.x + r.w / 2, r.y + r.h / 2 + 0.5f, "SAVE CURRENT AS...", nullptr);
                continue;
            }
            if (r.kind == 3) {             // search field
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(C_DISPLAY); fill(); strokeColor(query.empty() ? C_LINE : C_ACC); strokeWidth(1); stroke();
                beginPath(); circle(r.x + 12, r.y + r.h / 2, 4); moveTo(r.x + 15, r.y + r.h / 2 + 3); lineTo(r.x + 18, r.y + r.h / 2 + 6); strokeColor(C_DIM); strokeWidth(1.3f); stroke();
                fontSize(11); textLetterSpacing(0.3f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(query.empty() ? C_FAINT : C_TEXT);
                text(r.x + 26, r.y + r.h / 2 + 0.5f, query.empty() ? "Type to search names and categories" : (query + "|").c_str(), nullptr);
                continue;
            }
            const std::string key = favKey(r.kind, r.idx);
            const bool isCur = r.kind == 0 ? (curUser.empty() && r.idx == curFactory) : (r.kind == 1 ? curUser == userPresets[r.idx] : false);
            const bool del = r.kind == 1 && pendingDelete == r.idx;
            if (hot || del) { beginPath(); roundedRect(r.x, r.y, r.w, r.h, 4); fillColor(del ? Color(0xd0, 0x50, 0x40, 0.35f) : withAlpha(C_ACC, 0.18f)); fill(); }
            if (isCur) { beginPath(); circle(r.x + 6, r.y + r.h / 2, 2.5f); fillColor(C_ACC); fill(); }
            fontSize(11); textLetterSpacing(0.3f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(hot || isCur || del ? C_TEXT : C_DIM);
            const std::string label = del ? "Delete? click again" : itemLabel(r);
            text(r.x + 12, r.y + r.h / 2 + 0.5f, label.c_str(), nullptr);
            // the favourite star
            const bool fav = isFav(key);
            if (fav || hot) drawStar(r.x + r.w - 10, r.y + r.h / 2, 5.f, fav);
        }
    }
    void drawStar(float cx, float cy, float rr, bool filled) {
        beginPath();
        for (int k = 0; k < 10; ++k) { const float a = -3.14159265f / 2 + k * 3.14159265f / 5, q = k % 2 ? rr * 0.45f : rr; if (!k) moveTo(cx + q * std::cos(a), cy + q * std::sin(a)); else lineTo(cx + q * std::cos(a), cy + q * std::sin(a)); }
        closePath();
        if (filled) { fillColor(C_FAV); fill(); } else { strokeColor(C_DIM); strokeWidth(1); stroke(); }
    }
    int menuHit(float x, float y) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it);
        for (int i = 0; i < (int)it.size(); ++i) if (x >= it[i].x && x < it[i].x + it[i].w && y >= it[i].y && y < it[i].y + it[i].h) return i;
        return -1; }
    MRect menuItem(int i) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it); return it[i]; }
    void openMenu() { userPresets = store.list(); favs = store.favourites(); query.clear(); menuOpen = true; menuHover = -1; pendingDelete = -1; repaint(); }
    void applyFavourite(int idx) {
        if (idx >= (int)favs.size()) return;
        const std::string& k = favs[idx];
        if (k.compare(0, 2, "f:") == 0) { for (uint32_t i = 0; i < kNumPresets; ++i) if (k.substr(2) == presets()[i].name) { applyFactory((int)i); return; } }
        else applyUser(k.substr(2));
    }
    void drawSaveBox() {
        const float w = 360, h = 96, x = W / 2 - w / 2, y = 130;
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.45f)); fill();
        beginPath(); roundedRect(x, y, w, h, 8); fillColor(C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(9.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM); text(x + 16, y + 14, "SAVE PRESET AS", nullptr);
        beginPath(); roundedRect(x + 16, y + 36, w - 32, 26, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); stroke();
        fontSize(13); textLetterSpacing(0.4f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(x + 26, y + 49.5f, (saveName + "|").c_str(), nullptr);
        fontSize(8.5f); textLetterSpacing(1.2f); fillColor(C_FAINT); textAlign(ALIGN_RIGHT | ALIGN_TOP);
        text(x + w - 16, y + 72, saveError ? "COULD NOT SAVE - CHECK THE NAME" : "ENTER TO SAVE   ·   ESC TO CANCEL", nullptr);
    }

    // ---- analyser, meters, match capture (idle) ---------------------------------------------------
    int anaN() const { return 2048 << clampi((int)(val[kAnaRes] + 0.5f), 0, 3); }
    void spectrumOf(const float* ring, uint32_t w, int N, std::vector<cpx>& out) {
        fft.init(N);
        for (int i = 0; i < N; ++i) { const float hann = 0.5f - 0.5f * std::cos(2 * kPi * i / (N - 1)); out[i] = cpx(ring[(w - N + i) & (Engine::kAnaSize - 1)] * hann, 0.f); }
        fft.run(out.data(), false);
    }
    void updateAnalyser(const Engine& e, uint32_t w) {
        const int mode = (int)(val[kAnalyzer] + 0.5f); const bool ext = val[kAnaExternal] > 0.5f;
        const bool need[3] = { mode == 1 || mode == 3 || (ext && val[kCollision] > 0.5f && mode == 1), mode == 2 || mode == 3 || (ext && val[kCollision] > 0.5f && mode != 1), ext };
        const int N = anaN(); const float norm = 2.f / (N * 0.5f);
        static const float fall[5] = { 0.4f, 0.9f, 1.8f, 3.5f, 7.f }, rise[5] = { 0.12f, 0.3f, 0.6f, 0.9f, 1.f };
        const int sp = clampi((int)(val[kAnaSpeed] + 0.5f), 0, 4);
        const float tilt = val[kAnaTilt];
        const float* rings[3] = { e.anaPre, e.anaPost, e.anaSc };
        for (int s = 0; s < 3; ++s) {
            if (!need[s]) continue;
            if ((int)spec[s].size() != N / 2 + 1) spec[s].assign(N / 2 + 1, -150.f);
            spectrumOf(rings[s], w, N, buf);
            for (int k = 1; k <= N / 2; ++k) {
                const float db = 20.f * std::log10(std::max(std::abs(buf[k]) * norm, 1e-8f));
                float& v = spec[s][k];
                v = db > v ? v + (db - v) * rise[sp] : std::max(db, v - fall[sp]);
            }
            // display columns (max over the bins in each column), tilted around 1 kHz
            for (int i = 0; i <= COLS; ++i) {
                const float f0 = colHz(i), f1 = colHz(i + 1);
                int k0 = (int)(f0 / sampleRateUI * N), k1 = std::max(k0 + 1, (int)(f1 / sampleRateUI * N));
                k0 = clampi(k0, 1, N / 2 - 1); k1 = clampi(k1, k0 + 1, N / 2);
                float m = -150; for (int k = k0; k < k1; ++k) m = std::max(m, spec[s][k]);
                if (k1 - k0 <= 1 && k0 + 1 <= N / 2) {   // interpolate between bins where columns are narrower than a bin
                    const float pos = f0 / sampleRateUI * N, t = clampf(pos - (float)(int)pos, 0.f, 1.f);
                    const int kk = clampi((int)pos, 1, N / 2 - 1);
                    m = spec[s][kk] * (1 - t) + spec[s][std::min(kk + 1, N / 2)] * t;
                }
                col[s][i] = m + tilt * std::log2(std::max(f0, 1.f) / 1000.f);
            }
        }
        findPeaks(mode == 1 ? 0 : 1);
    }
    // local maxima that stand at least 6 dB above their surroundings: Spectrum Grab targets
    void findPeaks(int s) {
        peaks.clear();
        const int R = 10;
        for (int i = R; i <= COLS - R; ++i) {
            const float v = col[s][i]; if (v < -anaRangeDb() * 0.85f) continue;
            bool isMax = true; float minL = v, minR = v;
            for (int j = i - R; j <= i + R; ++j) { if (col[s][j] > v) { isMax = false; break; } if (j < i) minL = std::min(minL, col[s][j]); if (j > i) minR = std::min(minR, col[s][j]); }
            if (!isMax || v - std::max(minL, minR) < 6.f) continue;
            int l = i, r = i; while (l > 0 && col[s][l] > v - 3.f) --l; while (r < COLS && col[s][r] > v - 3.f) ++r;
            const float bw = std::max(0.05f, std::log2(colHz(r) / colHz(l)));
            const float pk = std::exp2(bw), q = clampf(std::sqrt(pk) / (pk - 1.f), 0.5f, 20.f);
            peaks.push_back({ colHz(i), v, q });
            i += R;
        }
    }
    void captureMatch(const Engine& e, uint32_t w) {
        if (w - matchLastW < (uint32_t)(MATCH_N / 2)) return;
        const float gap = (float)(w - matchLastW) / sampleRateUI; matchLastW = w;
        if (matchBufPre.size() != (size_t)MATCH_N) { matchBufPre.resize(MATCH_N); matchBufSc.resize(MATCH_N); }
        spectrumOf(e.anaPre, w, MATCH_N, matchBufPre); spectrumOf(e.anaSc, w, MATCH_N, matchBufSc);
        double ep = 0, es = 0; for (int k = 1; k <= MATCH_N / 2; ++k) { ep += std::norm(matchBufPre[k]); es += std::norm(matchBufSc[k]); }
        const double floor = 1e-7 * MATCH_N * MATCH_N;
        if (ep < floor || es < floor) { matchMsg = es < floor ? "No signal on the sidechain yet" : "No signal on the track yet"; return; }
        matchMsg.clear();
        for (int k = 0; k <= MATCH_N / 2; ++k) { matchPre[k] += std::norm(matchBufPre[k]); matchSc[k] += std::norm(matchBufSc[k]); }
        ++matchFrames; matchSecs += std::min(gap, 0.25f);
    }
    void applyMatch() {
        if (matchFrames == 0) return;
        std::vector<float> hz, tg;
        const float binHz = sampleRateUI / MATCH_N;
        double mean = 0; int nm = 0;
        for (int i = 0; i < NFIT; ++i) {
            const float f = 25.f * std::pow(18000.f / 25.f, (float)i / (NFIT - 1));
            const int k0 = std::max(1, (int)(f * std::exp2(-1.f / 6.f) / binHz)), k1 = std::max(k0 + 1, (int)(f * std::exp2(1.f / 6.f) / binHz));
            double a = 0, b = 0; for (int k = k0; k < k1 && k <= MATCH_N / 2; ++k) { a += matchPre[k]; b += matchSc[k]; }
            const float d = (float)(10.0 * std::log10((b + 1e-20) / (a + 1e-20)));
            hz.push_back(f); tg.push_back(d);
            if (f > 100 && f < 10000) { mean += d; ++nm; }
        }
        const float m = nm ? (float)(mean / nm) : 0.f;
        // smooth over about an octave: a tonal balance, not individual notes
        std::vector<float> sm(tg.size());
        for (size_t i = 0; i < tg.size(); ++i) { double a = 0; int n = 0; for (int j = -6; j <= 6; ++j) { const int k = (int)i + j; if (k >= 0 && k < (int)tg.size()) { a += tg[k]; ++n; } } sm[i] = clampf((float)(a / n) - m, -12.f, 12.f); }
        fitAndReplace(hz, sm, 12, 3.f, 15.f);
        matchOpen = false;
        flash("EQ Match applied: use GAIN SCALE to set the amount");
    }
    void fitAndReplace(const std::vector<float>& hz, const std::vector<float>& tg, int maxBands, float maxQ, float maxGain) {
        eq::CurveFitter cf; eq::FitOptions o; o.maxBands = maxBands; o.tolDb = 0.3f; o.fs = sampleRateUI; o.maxHz = 20000.f; o.maxQ = maxQ; o.maxGainDb = maxGain;
        // fit in unscaled gain (the display shows gain scale applied)
        const float gs = gainScaleOf(val); std::vector<float> t2(tg);
        if (std::fabs(gs) > 0.01f) for (float& v : t2) v /= gs;
        const std::vector<eq::FitBand> bs = cf.fit(hz, t2, o);
        beginGesture();
        for (int b = 0; b < kNumBands; ++b) if (used(b)) { commit(bp(b, kBUsed), 0); commit(bp(b, kBGain), 0); }
        for (bool& s : sel) s = false;
        focus = -1;
        for (const eq::FitBand& fb : bs) { const int b = newBand(fb.freq, fb.gainDb, fb.shape, fb.q); if (b < 0) break; }
        endGesture();
    }
    void applySketch() {
        int n = 0; for (float v : sketchDb) n += !std::isnan(v);
        if (n < 6) return;
        std::vector<float> hz, tg;
        for (int i = 0; i < NFIT; ++i) {
            const float f = 20.f * std::pow(1000.f, (float)i / (NFIT - 1));
            const int c = clampi((int)std::lround((xOfHz(f) - PL) / PW * COLS), 0, COLS);
            hz.push_back(f);
            tg.push_back(std::isnan(sketchDb[c]) ? totalDb(f, 0, false) : sketchDb[c]);
        }
        fitAndReplace(hz, tg, 12, 8.f, 30.f);
    }

    void uiIdle() override {
        const double t = nowSec();
        MicaPlugin* const dsp = (MicaPlugin*)getPluginInstancePointer();
        if (dsp && t - lastAna > 0.033) {
            lastAna = t;
            const Engine& e = dsp->engine;
            sampleRateUI = e.sampleRate();
            const uint32_t w = e.anaWrite.load(std::memory_order_acquire);
            const bool show = (int)(val[kAnalyzer] + 0.5f) != 0 || val[kAnaExternal] > 0.5f;
            if (show && w != lastWrite && val[kAnaFreeze] < 0.5f) { lastWrite = w; updateAnalyser(e, w); anaValid = true; }
            else if (show && w == lastWrite && anaValid && val[kAnaFreeze] < 0.5f) { for (int s = 0; s < 3; ++s) for (float& v : col[s]) v = std::max(-150.f, v - 1.5f); }
            if (matchListening) captureMatch(e, w);
            for (int b = 0; b < kNumBands; ++b) dynNow[b] = e.dynShow[b].load();
            specGain = e.spectralGain(); specBinHz = e.spectralBinHz(1);
            autoGainUI = e.autoGain();
            for (int c = 0; c < 2; ++c) {
                const float pk = const_cast<Engine&>(e).peakOut[c].exchange(0.f);
                const float db = 20.f * std::log10(std::max(pk, 1e-6f));
                meterDb[c] = std::max(db, meterDb[c] - 1.2f);
                meterHold = std::max(meterHold, db);
            }
            if (primary() != sentActive) { sentActive = primary(); char b[24]; snprintf(b, sizeof b, "active:%d", sentActive); setState("cmd", b); }
            repaint();
        }
        if (tips.due(gTipsOn, active >= 0 || dragDot >= 0 || dragHandle >= 0 || boxSel || menuOpen || saving || help.open || editing >= 0 || sketching)) repaint();
        if (flashMsg.until > 0 && !flashMsg.active()) { flashMsg.clear(); repaint(); }
    }
    bool tipContent(std::string& title, std::string& value, std::string& body) const {
        int k = tips.key, tipSeg = -1;
        if (k < 0) return false;
        if (k >= 100000) { tipSeg = (k - 100000) % 16; k = (k - 100000) / 16; }
        if (k < 1000) {
            const Ctl& c = controls()[k];
            if (c.type == DelButton) { title = targetTitle(kTipDelete); value.clear(); body = targetTip(kTipDelete); return true; }
            if (c.type == Hold) { title = targetTitle(kTipSolo); value.clear(); body = targetTip(kTipSolo); return true; }
            const uint32_t p = effParam(c); if (p >= kParamCount) return false;
            char b[40]; fmt(p, b, sizeof b);
            title = c.field == kFieldSlope ? "Slope" : paramDef(p).name; value = b; body = paramTip(p);
            if (c.field == kBShape && tipSeg >= 0) { body = std::string(shapeTip(tipSeg)) + "\n\n" + paramTip(p); }
            if (c.type == Knob) body += "\n\nDrag up/down or grab the rim and turn. Double-click to type a value, Cmd/Ctrl-click to reset.";
            if (paramCC[p] >= 0) { char cc[32]; snprintf(cc, sizeof cc, "   ·   CC%d", paramCC[p]); value += cc; }
            return true;
        }
        int tt = -1;
        if (k >= 7000) tt = kTipActiveFreq + (k - 7000);
        else if (k >= 6000) tt = k == 6000 ? kTipSketch : (k == 6001 ? kTipMatch : (k == 6002 ? kTipAnaMenu : (k == 6003 ? kTipGrab : (k == 6004 ? kTipMeter : kTipRangeHandle))));
        else if (k >= 5000) tt = kTipDisplay;
        else if (k >= 3000) tt = kTipDot;
        else if (k >= 2000) tt = kTipPrev + (k - 2000);
        else { const int hh = k - 1000;
            tt = hh == 1 ? kTipLearn : hh == 2 ? kTipMidi : hh == 3 ? kTipUndo : hh == 4 ? kTipRedo : hh == 5 ? kTipHelp : hh == 6 ? kTipA : hh == 7 ? kTipB : kTipCopy; }
        title = targetTitle(tt); value.clear(); body = targetTip(tt);
        if (tt == kTipDot) { char b[32]; snprintf(b, sizeof b, "Band %d", k - 3000 + 1); title = b; }
        if (tt == kTipUndo || tt == kTipRedo) { const size_t n = tt == kTipUndo ? history.undoSteps() : history.redoSteps(); char b[32]; snprintf(b, sizeof b, "%d step%s", (int)n, n == 1 ? "" : "s"); value = b; }
        if (tt >= kTipActiveFreq && activeCC[tt - kTipActiveFreq] >= 0) { char b[16]; snprintf(b, sizeof b, "CC%d", activeCC[tt - kTipActiveFreq]); value = b; }
        return true;
    }

    // ---- interaction --------------------------------------------------------------
    int hit(float x, float y, int* seg = nullptr) const {
        const std::vector<Ctl>& cs = controls();
        const bool menuOnly = anaMenu && inRect(x, y, AX, AY, AW, AH);
        for (int i = 0; i < (int)cs.size(); ++i) {
            const Ctl& c = cs[i];
            if (!visible(c)) continue;
            if (menuOnly != (c.group == GAna)) continue;
            switch (c.type) {
            case Knob: { const float rr = c.r + 6; if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= rr * rr) return i; break; }
            case BandToggle: if (x >= c.x - 18 && x <= c.x + 110 && std::fabs(y - c.y) <= 12) return i; break;
            case Switch: if (x >= c.x - 16 && x <= c.x + 96 && std::fabs(y - c.y) <= 10) return i; break;
            case DelButton: if (std::fabs(x - c.x) <= 13 && std::fabs(y - c.y) <= 11) return i; break;
            case Hold: if (std::fabs(x - c.x) <= c.r / 2 && std::fabs(y - c.y) <= SEG_H / 2) return i; break;
            case Stepper: { const float x0 = c.x - c.r / 2; if (x >= x0 && x < x0 + c.r && std::fabs(y - c.y) <= SEG_H / 2 + 1) { if (seg) *seg = x < c.x ? 0 : 1; return i; } break; }
            case Segment: { int n = 0; const int* m = nullptr; segNames(c, n, &m); const float x0 = c.x - c.r / 2;
                if (x >= x0 && x < x0 + c.r && std::fabs(y - c.y) <= SEG_H / 2 + 1) { if (seg) *seg = clampi((int)((x - x0) / (c.r / n)), 0, n - 1); return i; } break; }
            }
        }
        return -1;
    }
    static bool inRect(float x, float y, float rx, float ry, float rw, float rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }
    int headerHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H) return 0;
        if (x >= LEARN_X && x <= LEARN_X + LEARN_W) return 1;
        if (x >= MIDI_X && x <= MIDI_X + MIDI_W) return 2;
        if (x >= UNDO_X && x <= UNDO_X + UR_W) return 3;
        if (x >= REDO_X && x <= REDO_X + UR_W) return 4;
        if (x >= HELP_X && x <= HELP_X + HELP_W) return 5;
        if (x >= A_X && x < A_X + AB_W) return 6;
        if (x >= A_X + AB_W && x < A_X + 2 * AB_W) return 7;
        if (x >= COPY_X && x <= COPY_X + COPY_W) return 8;
        return 0;
    }
    int toolHit(float x, float y) const {
        if (y < TB_Y || y > TB_Y + TB_H) return 0;
        if (x >= SK_X && x < SK_X + SK_W) return 1;
        if (x >= MA_X && x < MA_X + MA_W) return 2;
        if (x >= AN_X && x < AN_X + AN_W) return 3;
        return 0;
    }
    int chipHit(float x, float y) const {
        if (!learnMode) return -1;
        for (int i = 0; i < 3; ++i) if (inRect(x, y, CH_X + i * 76, TB_Y + 1, 70, 18)) return i;
        return -1;
    }
    int presetBarHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H || x < PS_X || x > PS_X + PS_W) return -1;
        return x < PS_X + PS_ARROW ? 0 : (x > PS_X + PS_W - PS_ARROW ? 2 : 1);
    }
    bool clearHit(float x, float y) const { return learnMode && learnWaiting < 0 && inRect(x, y, DX + DW - 200, plotBottom() - 22, 190, 16); }
    bool inDisplay(float x, float y) const { return inRect(x, y, DX + 26, DY + 30, DW - 30, plotBottom() - DY - 30); }
    bool inMeter(float x, float y) const { return inRect(x, y, MX, DY, MW, DH); }
    int dotHit(float x, float y) const {
        int best = -1; float bd = 15 * 15;
        for (int b = 0; b < kNumBands; ++b) { if (!used(b)) continue; float dx, dy; dotPos(b, dx, dy); const float d = (x - dx) * (x - dx) + (y - dy) * (y - dy); if (d < bd) { bd = d; best = b; } }
        return best;
    }
    int handleHit(float x, float y) const {
        for (int b = 0; b < kNumBands; ++b) {
            if (!used(b) || !(sel[b] || b == focus || b == hoverDot || b == hoverHandle)) continue;
            float hx, hy; if (rangeHandlePos(b, hx, hy) && std::fabs(x - hx) < 8 && std::fabs(y - hy) < 8) return b;
        }
        return -1;
    }
    int peakHit(float x, float y) const {
        if (!anaValid || peaks.empty() || ((int)(val[kAnalyzer] + 0.5f) == 0)) return -1;
        int best = -1; float bd = 14;
        for (int i = 0; i < (int)peaks.size(); ++i) {
            const float px = xOfHz(peaks[i].hz), py = yOfAna(peaks[i].db);
            if (std::fabs(y - py) > 40) continue;
            const float d = std::fabs(x - px); if (d < bd) { bd = d; best = i; }
        }
        return best;
    }
    bool onCurve(float x, float y) const {
        const float hz = hzOfX(x);
        for (int c = 0; c < 2; ++c) if (std::fabs(yOfDb(totalDb(hz, c, false)) - y) < 7) return true;
        return false;
    }

    void setLearn(bool on) { learnMode = on; learnWaiting = -1; if (!on) setState("cmd", "cancel"); repaint(); }
    void endEdit(bool apply) {
        if (editing < 0) return;
        const uint32_t p = effParam(controls()[editing]); float v;
        if (apply && p < kParamCount && parse(editBuf, v)) { beginGesture(); applyToSelection(p, v, val[p]); endGesture(); }
        editing = -1; editBuf.clear(); repaint();
    }
    // set a band field on the focused band; other selected bands follow relatively (freq/Q/times by ratio, gains by offset) or take the same choice
    void applyToSelection(uint32_t p, float v, float old) {
        const int f = fieldOf(p), f2 = field2Of(p);
        if (f == kBShape) { const int b0 = bandOf(p); setShape(b0, (int)v); for (int b = 0; b < kNumBands; ++b) if (sel[b] && b != b0 && used(b)) setShape(b, (int)v); return; }
        setValue(p, v);
        if (f < 0 && f2 < 0) return;
        const float nv = val[p]; const int b0 = bandOf(p);
        const bool isSlope = (f == kBSlope) || (f2 == kB2Slope);
        for (int b = 0; b < kNumBands; ++b) {
            if (!sel[b] || b == b0 || !used(b)) continue;
            if (isSlope) { const int r = slopeRank((int)nv); const bool cut = isCutShape(shapeOf(b)); setValue(slopeParam(val, b), (float)slopeByRank(std::min(r, cut ? kNumSlopes - 1 : kNumSlopes - 2))); continue; }
            const uint32_t q = f >= 0 ? bp(b, f) : bp2(b, f2);
            const bool ratio = f == kBFreq || f == kBQ || f2 == kB2Attack || f2 == kB2Release || f2 == kB2ScFreq || f2 == kB2ScQ;
            const bool offset = f == kBGain || f2 == kB2Range || f2 == kB2Thresh;
            if (ratio) setValue(q, val[q] * nv / std::max(old, 1e-6f));
            else if (offset) setValue(q, val[q] + nv - old);
            else setValue(q, nv);
        }
    }
    void editSelection(bool begin) {
        for (int b = 0; b < kNumBands; ++b) if ((sel[b] || b == focus) && used(b)) {
            for (int f = 0; f < kBandFields; ++f) editParameter(bp(b, f), begin);
            for (int f = 0; f < kBand2Fields; ++f) editParameter(bp2(b, f), begin);
        }
    }
    static float angleNorm(const Ctl& c, float x, float y) {
        float a = std::atan2(y - c.y, x - c.x);
        const float a0 = 0.75f * 3.14159265f;
        float rel = a - a0; while (rel < 0) rel += 2 * 3.14159265f;
        if (rel > 1.5f * 3.14159265f) rel = rel > 1.75f * 3.14159265f ? 0.f : 1.5f * 3.14159265f;
        return rel / (1.5f * 3.14159265f);
    }
    float knobNorm(uint32_t p) const { return p == kGainScale ? (val[p] + 100.f) / 300.f : toNorm(p, val[p]); }
    float knobValue(uint32_t p, float n) const { return p == kGainScale ? std::round(clampf(n, 0.f, 1.f) * 300.f - 100.f) : fromNorm(p, n); }

    bool onKeyboard(const KeyboardEvent& ev) override {
        if (!ev.press) return false;
        std::string* buf2 = saving ? &saveName : (editing >= 0 ? &editBuf : nullptr);
        if (!buf2 && menuOpen) {                              // typing in the preset browser searches
            if (ev.key == kKeyEscape) { if (!query.empty()) query.clear(); else menuOpen = false; }
            else if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!query.empty()) query.pop_back(); }
            else if (ev.key >= 32 && ev.key < 127 && query.size() < 32) query += (char)ev.key;
            else return false;
            menuHover = -1; pendingDelete = -1; repaint(); return true;
        }
        if (!buf2) {
            const bool cmd = (ev.mod & (kModifierControl | kModifierSuper)) != 0;
            if (cmd && (ev.key == 'z' || ev.key == 'Z')) { if (ev.mod & kModifierShift) redo(); else undo(); return true; }
            if (cmd && (ev.key == 'y' || ev.key == 'Y')) { redo(); return true; }
            if (cmd && (ev.key == 'c' || ev.key == 'C')) { copyBands(); return true; }
            if (cmd && (ev.key == 'v' || ev.key == 'V')) { pasteBands(); return true; }
            if ((ev.key == kKeyDelete || ev.key == kKeyBackspace) && primary() >= 0 && !help.open) { deleteSelected(); return true; }
            if (ev.key == kKeyEscape && help.open) { help.open = false; repaint(); return true; }
            if (ev.key == kKeyEscape && anaMenu) { anaMenu = false; repaint(); return true; }
            if (ev.key == kKeyEscape && matchOpen) { matchOpen = false; matchListening = false; repaint(); return true; }
            if (ev.key == kKeyEscape && sketchMode) { sketchMode = false; repaint(); return true; }
            if (ev.key == kKeyEscape && learnMode) { setLearn(false); return true; }
            return false;
        }
        if (ev.key == kKeyEnter) {
            if (saving) {
                if (store.save(saveName, pparams, kParamCount, val)) { userPresets = store.list(); curUser = epsilon::PresetStore::sanitize(saveName); curFactory = -1; std::copy(val, val + kParamCount, userVals); saving = false; saveError = false; }
                else saveError = true;
            } else endEdit(true);
            repaint(); return true;
        }
        if (ev.key == kKeyEscape) { if (saving) saving = false; else endEdit(false); repaint(); return true; }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!buf2->empty()) buf2->pop_back(); repaint(); return true; }
        const char ch = epsilon::typedChar(ev.key, (ev.mod & kModifierShift) != 0);
        if (ch && buf2->size() < (saving ? 40u : 12u)) { *buf2 += ch; repaint(); return true; }
        return true;
    }

    // copy and paste bands as text (between Mica instances, or into a text file)
    void copyBands() {
        std::string t = "mica-bands 1\n";
        int n = 0;
        for (int b = 0; b < kNumBands; ++b) {
            if (!used(b) || !(selCount() == 0 || sel[b] || b == focus)) continue;
            char line[64];
            for (int f = 1; f < kBandFields; ++f) { snprintf(line, sizeof line, "%.9g ", val[bp(b, f)]); t += line; }
            for (int f = 0; f < kBand2Fields; ++f) { snprintf(line, sizeof line, "%.9g ", val[bp2(b, f)]); t += line; }
            t += "\n"; ++n;
        }
        if (n && setClipboard("text/plain", t.c_str(), t.size() + 1)) flash(n == 1 ? "Copied 1 band" : "Copied " + std::to_string(n) + " bands");
    }
    void pasteBands() {
        size_t sz = 0; const char* d = (const char*)getClipboard(sz);
        if (!d || sz < 12 || std::strncmp(d, "mica-bands 1", 12) != 0) { flash("Nothing to paste: copy Mica bands first"); return; }
        std::string s(d, strnlen(d, sz));
        beginGesture();
        for (bool& x : sel) x = false;
        focus = -1;
        size_t pos = s.find('\n'); int n = 0;
        while (pos != std::string::npos && pos + 1 < s.size()) {
            const size_t end = s.find('\n', pos + 1);
            const std::string line = s.substr(pos + 1, end == std::string::npos ? std::string::npos : end - pos - 1);
            pos = end;
            float v[kBandFields - 1 + kBand2Fields]; int k = 0; const char* q = line.c_str(); char* e = nullptr;
            while (k < (int)(sizeof v / sizeof v[0])) { const float x = std::strtof(q, &e); if (e == q) break; v[k++] = x; q = e; }
            if (k != (int)(sizeof v / sizeof v[0])) continue;
            int b = -1; for (int i = 0; i < kNumBands; ++i) if (!used(i)) { b = i; break; }
            if (b < 0) { flash("All 24 bands are in use"); break; }
            for (int f = 1; f < kBandFields; ++f) commit(bp(b, f), v[f - 1]);
            for (int f = 0; f < kBand2Fields; ++f) commit(bp2(b, f), v[kBandFields - 1 + f]);
            commit(bp(b, kBUsed), 1); sel[b] = true; focus = b; ++n;
        }
        endGesture();
        if (n) flash(n == 1 ? "Pasted 1 band" : "Pasted " + std::to_string(n) + " bands");
    }
    void startDotDrag(int d, float x, float y) {
        beginGesture(); dragDot = d; dragX0 = x; dragY0 = y;
        for (int b = 0; b < kNumBands; ++b) { startF[b] = val[bp(b, kBFreq)]; startG[b] = val[bp(b, kBGain)]; startQ[b] = val[bp(b, kBQ)]; }
        editSelection(true); repaint();
    }
    void setSolo(int b) { soloing = b >= 0; char s[24]; snprintf(s, sizeof s, "solo:%d", b); setState("cmd", s); repaint(); }

    bool onMouse(const MouseEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (ev.press) { flashMsg.clear(); tips.press(); }
        if (help.open) { if (ev.press && ev.button == 1) { help.click(x, y, gTipsOn); repaint(); } return true; }
        if (!ev.press && soloing) { setSolo(-1); return true; }
        if (ev.button == 2 && ev.press) {
            if (menuOpen) { const int m = menuHit(x, y); if (m >= 0 && menuItem(m).kind == 1) { pendingDelete = menuItem(m).idx; repaint(); } return true; }
            if (learnMode) {
                const int ch = chipHit(x, y);
                if (ch >= 0) { char b[24]; snprintf(b, sizeof b, "unmap:%d", MicaPlugin::kActiveTarget + ch); setState("cmd", b); activeCC[ch] = -1; repaint(); return true; }
                const int i = hit(x, y); if (i >= 0) { const uint32_t p = effParam(controls()[i]); if (p < kParamCount) { char b[24]; snprintf(b, sizeof b, "unmap:%u", p); setState("cmd", b); paramCC[p] = -1; repaint(); } } return true; }
            const int d = dotHit(x, y);
            if (d >= 0) { if (!sel[d]) selectOnly(d); deleteSelected(); return true; }
            return false;
        }
        if (ev.button != 1) return false;
        if (!ev.press) {
            if (active >= 0) { const uint32_t p = effParam(controls()[active]); if (controls()[active].group == GBand || controls()[active].group == GDyn) editSelection(false); else if (p < kParamCount) editParameter(p, false); active = -1; endGesture(); return true; }
            if (dragDot >= 0) { editSelection(false); dragDot = -1; endGesture(); return true; }
            if (dragHandle >= 0) { editParameter(bp2(dragHandle, kB2Range), false); dragHandle = -1; endGesture(); return true; }
            if (sketching) { sketching = false; applySketch(); std::fill(sketchDb.begin(), sketchDb.end(), NAN); repaint(); return true; }
            if (boxSel) {
                boxSel = false;
                const float x0 = std::min(boxX0, boxX1), x1 = std::max(boxX0, boxX1), y0 = std::min(boxY0, boxY1), y1 = std::max(boxY0, boxY1);
                if (!(ev.mod & kModifierShift)) { for (bool& sb : sel) sb = false; focus = -1; }
                for (int b = 0; b < kNumBands; ++b) { if (!used(b)) continue; float dx, dy; dotPos(b, dx, dy); if (dx >= x0 && dx <= x1 && dy >= y0 && dy <= y1) { sel[b] = true; focus = b; } }
                repaint(); return true;
            }
            return false;
        }
        if (editing >= 0) endEdit(true);
        if (menuOpen) {
            const int m = menuHit(x, y);
            if (m >= 0) {
                const MRect r = menuItem(m);
                if (r.kind == 3) return true;                       // the search field: just type
                if ((r.kind == 0 || r.kind == 1 || r.kind == 4) && x >= r.x + r.w - 20) { toggleFav(favKey(r.kind, r.idx)); return true; }
                if (r.kind == 2) { menuOpen = false; saving = true; saveError = false; saveName = curUser; }
                else if (r.kind == 1 && pendingDelete == r.idx) { store.remove(userPresets[r.idx]); if (curUser == userPresets[r.idx]) curUser.clear(); userPresets = store.list(); pendingDelete = -1; repaint(); return true; }
                else { menuOpen = false; if (r.kind == 0) applyFactory(r.idx); else if (r.kind == 1) applyUser(userPresets[r.idx]); else applyFavourite(r.idx); }
            } else menuOpen = false;
            pendingDelete = -1; menuHover = -1; repaint(); return true;
        }
        if (matchOpen) {
            const int m = matchHit(x, y);
            if (m == 0) { matchListening = !matchListening; if (matchListening) { if (MicaPlugin* dsp = (MicaPlugin*)getPluginInstancePointer()) matchLastW = dsp->engine.anaWrite.load(); } repaint(); return true; }
            if (m == 1 && matchFrames > 0) { matchListening = false; applyMatch(); return true; }
            if (m == 2) { matchOpen = false; matchListening = false; repaint(); return true; }
            if (m == -1) return true;
        }
        const int th = toolHit(x, y);
        if (th == 1) { sketchMode = !sketchMode; anaMenu = false; repaint(); return true; }
        if (th == 2) { matchOpen = !matchOpen; matchListening = false; if (matchOpen) { std::fill(matchPre.begin(), matchPre.end(), 0.0); std::fill(matchSc.begin(), matchSc.end(), 0.0); matchFrames = 0; matchSecs = 0; matchMsg.clear(); } anaMenu = false; repaint(); return true; }
        if (th == 3) { anaMenu = !anaMenu; repaint(); return true; }
        if (anaMenu && !inRect(x, y, AX, AY, AW, AH)) { anaMenu = false; repaint(); }
        const int hh = headerHit(x, y);
        if (hh == 1) { setLearn(!learnMode); return true; }
        if (hh == 2) { beginGesture(); commit(kMidiOn, val[kMidiOn] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
        if (hh == 3) { undo(); return true; }
        if (hh == 4) { redo(); return true; }
        if (hh == 5) { help.open = true; repaint(); return true; }
        if (hh == 6 || hh == 7) { switchAB(hh - 6); return true; }
        if (hh == 8) { copyAB(); return true; }
        const int pb = presetBarHit(x, y);
        if (pb == 0) { step(-1); return true; }
        if (pb == 2) { step(1); return true; }
        if (pb == 1) { openMenu(); return true; }
        if (clearHit(x, y)) { setState("cmd", "clear"); for (int& m : paramCC) m = -1; for (int& m : activeCC) m = -1; repaint(); return true; }
        if (inMeter(x, y)) { meterHold = -120; repaint(); return true; }
        const int ch = chipHit(x, y);
        if (ch >= 0) { learnWaiting = MicaPlugin::kActiveTarget + ch; char b[24]; snprintf(b, sizeof b, "learn:%d", learnWaiting); setState("cmd", b); repaint(); return true; }

        int seg = 0; const int i = hit(x, y, &seg);
        if (i >= 0) {
            const Ctl& c = controls()[i]; const uint32_t p = effParam(c);
            if (c.type == DelButton) { deleteSelected(); return true; }
            if (c.type == Hold) { if (primary() >= 0) setSolo(primary()); return true; }
            if (p >= kParamCount) return true;
            if (learnMode) { learnWaiting = (int)p; char b[24]; snprintf(b, sizeof b, "learn:%u", p); setState("cmd", b); repaint(); return true; }
            if (dimmed(c) && c.type != Knob) return true;
            if (c.type == BandToggle || c.type == Switch) { beginGesture(); applyToSelection(p, val[p] > 0.5f ? 0.f : 1.f, val[p]); endGesture(); return true; }
            if (c.type == Segment) { int n = 0; const int* map = nullptr; segNames(c, n, &map); beginGesture(); applyToSelection(p, (float)(map ? map[seg] : seg), val[p]); endGesture(); return true; }
            if (c.type == Stepper) {
                beginGesture();
                if (c.field == kFieldSlope) { stepSlope(primary(), seg == 0 ? -1 : 1); applyToSelection(slopeParam(val, primary()), val[slopeParam(val, primary())], 0); }
                else setValue(p, val[p] + (seg == 0 ? -0.5f : 0.5f));
                endGesture(); return true;
            }
            if (ev.mod & (kModifierControl | kModifierSuper)) {
                float d = defv[p];
                if (c.field == kBFreq) d = val[p];
                if (c.field == kBQ) d = isCutShape(shapeOf(primary())) ? 0.70710678f : 1.f;
                beginGesture(); applyToSelection(p, d, val[p]); endGesture(); return true;
            }
            if (i == lastClick && ev.time - lastClickTime < 350) {
                lastClick = -1; editing = i;
                char b[40]; fmt(p, b, sizeof b); editBuf = b;
                const size_t sp = editBuf.find(' '); if (sp != std::string::npos) editBuf.resize(sp);
                if (!editBuf.empty() && editBuf.back() == '%') editBuf.pop_back();
                if ((fieldOf(p) == kBFreq || field2Of(p) == kB2ScFreq) && val[p] >= 1000) { char kb[16]; snprintf(kb, sizeof kb, "%.3gk", val[p] / 1000); editBuf = kb; }
                repaint(); return true;
            }
            lastClick = i; lastClickTime = ev.time;
            beginGesture();
            active = i; dragY = y; dragNorm = knobNorm(p);
            circular = c.r > 19 && std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y)) > c.r * 0.62f;
            if (c.group == GBand || c.group == GDyn) editSelection(true); else editParameter(p, true);
            if (circular) applyToSelection(p, knobValue(p, angleNorm(c, x, y)), val[p]);
            repaint(); return true;
        }
        if (!learnMode && inDisplay(x, y)) {
            if (sketchMode) { sketching = true; std::fill(sketchDb.begin(), sketchDb.end(), NAN); lastSketchCol = -1; sketchAt(x, y); return true; }
            const int hd = handleHit(x, y);
            if (hd >= 0) { beginGesture(); dragHandle = hd; dragY0 = y; startG[hd] = val[bp2(hd, kB2Range)]; editParameter(bp2(hd, kB2Range), true); repaint(); return true; }
            const int d = dotHit(x, y);
            if (d >= 0) {
                if (ev.mod & kModifierAlt) { beginGesture(); commit(bp(d, kBOn), val[bp(d, kBOn)] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
                if (ev.mod & (kModifierShift | kModifierControl | kModifierSuper)) { sel[d] = !sel[d]; focus = sel[d] ? d : (focus == d ? -1 : focus); repaint(); return true; }
                if (!sel[d]) selectOnly(d); else focus = d;
                startDotDrag(d, x, y); return true;
            }
            const int pk = peakHit(x, y);
            if (pk >= 0) {     // Spectrum Grab: a band on the peak, then drag for gain
                beginGesture();
                const int b = newBand(peaks[pk].hz, 0.f, kBell, peaks[pk].q);
                if (b < 0) { endGesture(); return true; }
                endGesture(); selectOnly(b); grabPeak = -1;
                startDotDrag(b, x, yOfDb(0)); return true;
            }
            const bool dbl = ev.time - lastDispClick < 350 && std::fabs(x - lastDispX) < 6 && std::fabs(y - lastDispY) < 6;
            lastDispClick = ev.time; lastDispX = x; lastDispY = y;
            if (dbl || onCurve(x, y)) {
                beginGesture();
                const int b = newBand(hzOfX(x), dbOfY(y) / std::max(0.01f, std::fabs(gainScaleOf(val))) * (gainScaleOf(val) < 0 ? -1.f : 1.f));
                if (b < 0) { endGesture(); return true; }
                endGesture(); selectOnly(b);
                startDotDrag(b, x, y); return true;
            }
            boxSel = true; boxX0 = boxX1 = x; boxY0 = boxY1 = y; repaint(); return true;
        }
        return false;
    }
    void sketchAt(float x, float y) {
        const int c = clampi((int)std::lround((x - PL) / PW * COLS), 0, COLS);
        const float db = clampf(dbOfY(y), -30.f, 30.f);
        if (lastSketchCol >= 0 && lastSketchCol != c) {
            const int a = std::min(c, lastSketchCol), z = std::max(c, lastSketchCol);
            for (int k = a; k <= z; ++k) { const float t = (float)(k - lastSketchCol) / (float)(c - lastSketchCol); sketchDb[k] = lastSketchDb + (db - lastSketchDb) * t; }
        } else sketchDb[c] = db;
        lastSketchCol = c; lastSketchDb = db; repaint();
    }

    bool onMotion(const MotionEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (help.open) { if (help.motion(x, y)) repaint(); return true; }
        if (active >= 0) {
            const Ctl& c = controls()[active]; const uint32_t p = effParam(c);
            if (p >= kParamCount) return true;
            if (circular) { applyToSelection(p, knobValue(p, angleNorm(c, x, y)), val[p]); return true; }
            float sens = (ev.mod & kModifierShift) ? 1500.f : 240.f;
            if (paramDef(p).hints & kHintInteger) sens = 80.f;
            dragNorm = clampf(dragNorm + (dragY - y) / sens, 0.f, 1.f); dragY = y;
            applyToSelection(p, knobValue(p, dragNorm), val[p]);
            return true;
        }
        if (sketching) { sketchAt(x, y); return true; }
        if (dragHandle >= 0) {
            const float gs = gainScaleOf(val); const float fine = (ev.mod & kModifierShift) ? 0.15f : 1.f;
            const float dDb = (dbOfY(dragY0 + (y - dragY0) * fine) - dbOfY(dragY0)) / (std::fabs(gs) > 0.01f ? gs : 1.f);
            setValue(bp2(dragHandle, kB2Range), clampf(startG[dragHandle] + dDb, -30.f, 30.f));
            return true;
        }
        if (dragDot >= 0) {
            const float fine = (ev.mod & kModifierShift) ? 0.15f : 1.f, gs = gainScaleOf(val);
            const float ratio = hzOfX(dragX0 + (x - dragX0) * fine) / hzOfX(dragX0), dDb = (dbOfY(dragY0 + (y - dragY0) * fine) - dbOfY(dragY0));
            for (int b = 0; b < kNumBands; ++b) {
                if (!(sel[b] || b == dragDot) || !used(b)) continue;
                float nf = clampf(startF[b] * ratio, FMIN, FMAX);
                if (val[kPiano] > 0.5f && !(ev.mod & kModifierShift) && b == dragDot) nf = 440.f * std::exp2(std::round(12.f * std::log2(nf / 440.f)) / 12.f);   // piano display: snap to notes
                else if (val[kPiano] > 0.5f && !(ev.mod & kModifierShift)) nf = clampf(startF[b] * (440.f * std::exp2(std::round(12.f * std::log2(startF[dragDot] * ratio / 440.f)) / 12.f)) / startF[dragDot], FMIN, FMAX);
                setValue(bp(b, kBFreq), clampf(nf, FMIN, FMAX));
                if (!eq::hasGain(shapeOf(b))) setValue(bp(b, kBQ), clampf(startQ[b] * std::pow(10.f, dDb / 20.f), 0.1f, 40.f));
                else setValue(bp(b, kBGain), clampf(startG[b] + dDb / (std::fabs(gs) > 0.01f ? gs : 1.f), -30.f, 30.f));
                if (val[bp2(b, kB2ScFree)] < 0.5f) setValue(bp2(b, kB2ScFreq), val[bp(b, kBFreq)]);
            }
            return true;
        }
        if (boxSel) { boxX1 = x; boxY1 = y; repaint(); return true; }
        if (menuOpen) { const int m = menuHit(x, y); if (m != menuHover) { menuHover = m; repaint(); } return true; }
        const int mh = matchHit(x, y); if ((mh >= 0 ? mh : -1) != hoverMatch) { hoverMatch = mh >= 0 ? mh : -1; repaint(); }
        const int tl = toolHit(x, y); if (tl != hoverTool) { hoverTool = tl; repaint(); }
        const int hh = headerHit(x, y); if (hh != hoverHeader) { hoverHeader = hh; repaint(); }
        const int pb = presetBarHit(x, y); if (pb != presetHover) { presetHover = pb; repaint(); }
        const bool hc = clearHit(x, y); if (hc != hoverClear) { hoverClear = hc; repaint(); }
        const int chh = chipHit(x, y); if (chh != hoverChip) { hoverChip = chh; repaint(); }
        int seg = -1; const int h = (matchOpen && mh != -2) ? -1 : hit(x, y, &seg);
        if (h != hover || seg != hoverSeg) { hover = h; hoverSeg = seg; repaint(); }
        const bool disp = h < 0 && !learnMode && inDisplay(x, y) && !(anaMenu && inRect(x, y, AX, AY, AW, AH)) && !(matchOpen && mh != -2) && !tl;
        const int hd = disp ? dotHit(x, y) : -1; if (hd != hoverDot) { hoverDot = hd; repaint(); }
        const int hhd = disp ? handleHit(x, y) : -1; if (hhd != hoverHandle) { hoverHandle = hhd; repaint(); }
        const int gp = disp && hd < 0 && hhd < 0 && !sketchMode ? peakHit(x, y) : -1; if (gp != grabPeak) { grabPeak = gp; repaint(); }
        int key = -1;
        if (h >= 0) key = h;
        else if (tl > 0) key = 6000 + tl - 1;
        else if (chh >= 0) key = 7000 + chh;
        else if (hh > 0) key = 1000 + hh;
        else if (pb >= 0) key = 2000 + pb;
        else if (hhd >= 0) key = 6005;
        else if (hd >= 0) key = 3000 + hd;
        else if (gp >= 0) key = 6003;
        else if (inMeter(x, y)) key = 6004;
        else if (disp) key = 5000;
        if (h >= 0 && controls()[h].field == kBShape && seg >= 0) key = 100000 + h * 16 + seg;   // each shape has its own tip
        if (tips.hover(key, x, y)) repaint();
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override {
        const float s = uiScale(), sx = ev.pos.getX() / s, sy = ev.pos.getY() / s;
        if (menuOpen || saving || help.open) return true;
        if (presetBarHit(sx, sy) >= 0) { step(ev.delta.getY() > 0 ? -1 : 1); return true; }
        const float dir = ev.delta.getY() > 0 ? 1.f : -1.f;
        int seg = 0; const int i = hit(sx, sy, &seg);
        if (i >= 0) {
            const Ctl& c = controls()[i]; const uint32_t p = effParam(c); if (p >= kParamCount) return false;
            if (c.type == Stepper) { beginGesture(); if (c.field == kFieldSlope) stepSlope(primary(), dir > 0 ? 1 : -1); else setValue(p, val[p] + dir * 0.5f); endGesture(); return true; }
            if (c.type != Knob) return false;
            const float st = (ev.mod & kModifierShift) ? 0.004f : 0.02f;
            beginGesture(); applyToSelection(p, knobValue(p, knobNorm(p) + dir * st), val[p]); endGesture();
            return true;
        }
        if (inDisplay(sx, sy)) {          // mouse wheel over a dot: Q (of the whole selection if the dot is selected)
            const int d = dotHit(sx, sy); if (d < 0) return false;
            if (!sel[d]) selectOnly(d);
            const float k = std::pow(2.f, dir * ((ev.mod & kModifierShift) ? 0.05f : 0.2f));
            beginGesture(); for (int b = 0; b < kNumBands; ++b) if ((sel[b] || b == d) && used(b)) commit(bp(b, kBQ), clampf(val[bp(b, kBQ)] * k, 0.025f, 40.f)); endGesture();
            return true;
        }
        return false;
    }

private:
    struct Peak { float hz, db, q; };
    epsilon::Theme theme;
    epsilon::HelpWindow help;
    epsilon::PresetStore store;
    epsilon::PresetParam pparams[kParamCount];
    epsilon::UndoHistory history;
    epsilon::TipTimer tips;
    epsilon::Flash flashMsg;
    float val[kParamCount], defv[kParamCount], userVals[kParamCount] = {};
    int paramCC[kParamCount], activeCC[3];
    bool sel[kNumBands]; int focus = -1, sentActive = -2;
    int active = -1, hover = -1, hoverSeg = -1, lastClick = -1, editing = -1, hoverDot = -1, dragDot = -1, hoverHandle = -1, dragHandle = -1;
    uint lastClickTime = 0, lastDispClick = 0; float lastDispX = 0, lastDispY = 0;
    float dragY = 0, dragNorm = 0, dragX0 = 0, dragY0 = 0; bool circular = false, gesture = false;
    float startF[kNumBands] = {}, startG[kNumBands] = {}, startQ[kNumBands] = {};
    bool boxSel = false; float boxX0 = 0, boxY0 = 0, boxX1 = 0, boxY1 = 0;
    // drawing caches
    eq::Design designs[kNumBands], designsRange[kNumBands], designsNow[kNumBands]; bool designOk[kNumBands] = {};
    float dynNow[kNumBands] = {};
    std::vector<float> specGain; float specBinHz = 46.875f, autoGainUI = 0;
    // analyser
    epsilon::Fft fft; std::vector<cpx> buf; std::vector<float> spec[3], col[3]; bool anaValid = false; uint32_t lastWrite = 0; double lastAna = 0; float sampleRateUI = 48000.f;
    std::vector<Peak> peaks; int grabPeak = -1;
    bool anaMenu = false;
    // meter
    float meterDb[2] = { -120, -120 }, meterHold = -120;
    // sketch
    bool sketchMode = false, sketching = false; std::vector<float> sketchDb; int lastSketchCol = -1; float lastSketchDb = 0;
    // match
    bool matchOpen = false, matchListening = false; std::vector<double> matchPre, matchSc; std::vector<cpx> matchBufPre, matchBufSc;
    int matchFrames = 0; float matchSecs = 0; uint32_t matchLastW = 0; std::string matchMsg; int hoverMatch = -1;
    // solo, tools
    bool soloing = false; int hoverTool = 0, hoverChip = -1;
    std::string editBuf;
    int curFactory = 0, presetHover = -1, menuHover = -1, pendingDelete = -1, hoverHeader = 0;
    std::string curUser, query; std::vector<std::string> userPresets, favs;
    bool menuOpen = false, saving = false, saveError = false; std::string saveName;
    bool learnMode = false, hoverClear = false; int learnWaiting = -1; float lastLearnEcho = 0;
    ResizeHandle resizeHandle;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MicaUI)
};

UI* createUI() { return new MicaUI(); }

END_NAMESPACE_DISTRHO
