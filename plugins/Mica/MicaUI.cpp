// Mica — custom vector UI (DPF/NanoVG). Original design. License: MIT.
#include "DistrhoUI.hpp"
#include "MicaPlugin.hpp"
#include "UserPresets.hpp"
#include "ResizeHandle.hpp"
#include "Help.hpp"
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
extern "C" __declspec(dllimport) void* __stdcall ShellExecuteA(void*, const char*, const char*, const char*, const char*, int);
#endif

START_NAMESPACE_DISTRHO

static void openURL(const char* url) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, 1);
#elif defined(__APPLE__)
    const std::string c = std::string("open '") + url + "' &"; if (std::system(c.c_str())) {}
#else
    const std::string c = std::string("xdg-open '") + url + "' >/dev/null 2>&1 &"; if (std::system(c.c_str())) {}
#endif
}
static bool gTipsOn = true;
static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

using namespace mica;
using DGL_NAMESPACE::ResizeHandle;

static constexpr float W = 1100.f, H = 680.f;

#define C_BG        Color(0x0f, 0x11, 0x13)
#define C_PANEL     Color(0x18, 0x1b, 0x1e)
#define C_PANEL_HI  Color(0x23, 0x27, 0x2b)
#define C_LINE      Color(0x2d, 0x32, 0x37)
#define C_TEXT      Color(0xe6, 0xea, 0xec)
#define C_DIM       Color(0x88, 0x90, 0x97)
#define C_FAINT     Color(0x4a, 0x51, 0x58)
#define C_ACC       Color(0x9f, 0xd8, 0xe0)       // mica: pale silvery cyan
#define C_ANA       Color(0x5a, 0x7a, 0x8c)
#define C_DISPLAY   Color(0x0a, 0x0c, 0x0d)

static Color withAlpha(const Color& c, float a) { Color r(c); r.alpha = a; return r; }
static Color bandColor(int b) {
    // evenly spaced hues, softened
    const float h = std::fmod(b * 0.1618034f * 6.f, 6.f), x = 1.f - std::fabs(std::fmod(h, 2.f) - 1.f);
    float r = 0, g = 0, bl = 0;
    if (h < 1) { r = 1; g = x; } else if (h < 2) { r = x; g = 1; } else if (h < 3) { g = 1; bl = x; } else if (h < 4) { g = x; bl = 1; } else if (h < 5) { r = x; bl = 1; } else { r = 1; bl = x; }
    return Color(0.45f + 0.5f * r, 0.45f + 0.5f * g, 0.45f + 0.5f * bl);
}

// ---------------------------------------------------------------- layout
static constexpr float DX = 16, DY = 64, DW = 1068, DH = 420;           // EQ display
static constexpr float PY = 492, PH = 172;                               // bottom panels
static constexpr float BX = 16, BW = 640, GX = 664, GW = 420;            // band panel, global panel
static constexpr float SEG_H = 24;
static constexpr float FMIN = 10.f, FMAX = 30000.f;

enum WType { Knob, Toggle, Segment, DelButton };
// field >= 0: a control of the selected band (param = bp(sel, field)); field < 0: a global param
struct Ctl { WType type; int field; uint32_t param; float x, y, r; const char* label; };

static std::vector<Ctl> buildControls() {
    std::vector<Ctl> c;
    c.push_back({ Toggle, kBOn, 0, BX + 30, PY + 24, 0, "BAND" });
    c.push_back({ DelButton, -1, 0, BX + BW - 26, PY + 24, 0, "" });
    c.push_back({ Segment, kBShape, 0, BX + 354, PY + 24, 390, "" });
    c.push_back({ Segment, kBSlope, 0, BX + 136, PY + 66, 216, "" });
    c.push_back({ Segment, kBPlace, 0, BX + 448, PY + 66, 300, "" });
    c.push_back({ Knob, kBFreq, 0, BX + 150, PY + 118, 28, "FREQUENCY" });
    c.push_back({ Knob, kBGain, 0, BX + 320, PY + 118, 28, "GAIN" });
    c.push_back({ Knob, kBQ,    0, BX + 490, PY + 118, 28, "Q" });
    c.push_back({ Segment, -1, kProcMode,    GX + 140, PY + 36, 236, "" });
    c.push_back({ Segment, -1, kLinQuality,  GX + 340, PY + 36, 132, "" });
    c.push_back({ Segment, -1, kChannelMode, GX + 140, PY + 88, 236, "" });
    c.push_back({ Segment, -1, kAnalyzer,    GX + 340, PY + 88, 132, "" });
    c.push_back({ Segment, -1, kRange,       GX + 140, PY + 140, 236, "" });
    c.push_back({ Knob, -1, kOutGain,        GX + 340, PY + 132, 20, "OUTPUT" });
    return c;
}
static const std::vector<Ctl>& controls() { static std::vector<Ctl> c = buildControls(); return c; }

static constexpr float PS_X = 300.f, PS_Y = 20.f, PS_W = 250.f, PS_H = 28.f, PS_ARROW = 30.f;
static constexpr float A_X = 562.f, AB_W = 30.f, COPY_X = 626.f, COPY_W = 44.f;
static constexpr float UNDO_X = 684.f, REDO_X = 738.f, UR_W = 50.f, LEARN_X = 798.f, LEARN_W = 66.f, MIDI_X = 870.f, MIDI_W = 52.f, HELP_X = 928.f, HELP_W = 48.f;
static constexpr float PM_COLW = 160.f, PM_ROW = 23.f, PM_HEAD = 22.f, PM_PAD = 8.f;
static constexpr int ANA_N = 4096;

// combined response of the parameter set for channel A (0) or B (1)
static cpx respAt(const float* v, float hz, int c) {
    cpx h(1.f, 0.f);
    for (int b = 0; b < kNumBands; ++b) {
        if (v[bp(b, kBUsed)] < 0.5f || v[bp(b, kBOn)] < 0.5f) continue;
        const int pl = (int)(v[bp(b, kBPlace)] + 0.5f);
        if (!(pl == kPlaceBoth || (pl == kPlaceA && c == 0) || (pl == kPlaceB && c == 1))) continue;
        h *= bandResponse((int)(v[bp(b, kBShape)] + .5f), (int)(v[bp(b, kBSlope)] + .5f), v[bp(b, kBFreq)], v[bp(b, kBGain)], v[bp(b, kBQ)], hz);
    }
    return h;
}
static bool isCut(int shape) { return shape == kLowCut || shape == kHighCut; }

class MicaUI : public UI {
public:
    MicaUI() : UI((uint)W, (uint)H), resizeHandle(this) {
        loadSharedResources();
        Engine def;
        for (uint32_t i = 0; i < kParamCount; ++i) { val[i] = def.p[i]; defv[i] = def.p[i]; }
        for (int& m : paramCC) m = -1;
        const double sf = getScaleFactor();
        if (sf != 1.0) setSize((uint)(W * sf), (uint)(H * sf));
        setGeometryConstraints((uint)(W * 0.6 * sf), (uint)(H * 0.6 * sf), true);
        if (isResizable()) resizeHandle.hide();
        userPresets = listUserPresets();
        std::copy(val, val + kParamCount, before);
        fft.init(ANA_N); spec.assign(ANA_N / 2, -120.f); buf.resize(ANA_N);
        for (bool& s : sel) s = false;
    }

protected:
    float uiScale() const { return (float)getWidth() / W; }

    // ---- values -------------------------------------------------------------
    void setValue(uint32_t p, float v) {
        const ParamDef& d = paramDef(p);
        v = clampf(v, d.min, d.max);
        if (v == val[p]) return;
        val[p] = v; setParameterValue(p, v); repaint();
    }
    void commit(uint32_t p, float v) { editParameter(p, true); setValue(p, v); editParameter(p, false); }
    void parameterChanged(uint32_t i, float v) override {
        if (i >= kParamCount) return;
        if (i == kOutLearn) {
            if (v != lastLearnEcho && v > 0) {
                const int code = (int)v % 1000000 - 1, prm = code / 128, cc = code % 128;
                for (int& m : paramCC) if (m == cc) m = -1;
                if (prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
                learnWaiting = -1;
            }
            lastLearnEcho = v; repaint(); return;
        }
        if (val[i] != v) { val[i] = v; if (!gesture) before[i] = v; repaint(); }
    }
    void stateChanged(const char* key, const char* value) override {
        if (std::strcmp(key, "midimap") != 0) return;
        for (int& m : paramCC) m = -1;
        const char* q = value;
        while (q && *q) {
            int cc = -1, prm = -1;
            if (std::sscanf(q, "%d:%d", &cc, &prm) == 2 && prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
            const char* s = std::strchr(q, ';'); if (!s) break; q = s + 1;
        }
        repaint();
    }
    bool used(int b) const { return val[bp(b, kBUsed)] > 0.5f; }
    int primary() const { return (focus >= 0 && used(focus)) ? focus : -1; }
    int selCount() const { int n = 0; for (int b = 0; b < kNumBands; ++b) n += sel[b] && used(b); return n; }

    // ---- undo / redo / A-B -----------------------------------------------------------
    typedef std::vector<float> Snap;
    Snap snapshot() const { Snap s(val, val + kParamCount); s.push_back((float)abSlot); return s; }
    bool differs(const float* a, const float* b) const {
        for (uint32_t i = 0; i < kParamCount; ++i) if (!(paramDef(i).hints & kHintOutput) && a[i] != b[i]) return true;
        return false;
    }
    void beginGesture() { gesture = true; std::copy(val, val + kParamCount, before); }
    void endGesture() {
        gesture = false;
        if (differs(before, val)) { Snap u(before, before + kParamCount); u.push_back((float)abSlot); undoStack.push_back(u); if (undoStack.size() > 200) undoStack.erase(undoStack.begin()); redoStack.clear(); }
        std::copy(val, val + kParamCount, before); repaint();
    }
    void applyAll(const float* s) { for (uint32_t i = 0; i < kParamCount; ++i) if (!(paramDef(i).hints & kHintOutput) && val[i] != s[i]) commit(i, s[i]); }
    void restoreSnap(const Snap& s) {
        if (s.size() > kParamCount && (int)s[kParamCount] != abSlot) { abStore[abSlot] = snapshot(); abSlot = (int)s[kParamCount]; }
        applyAll(s.data()); std::copy(val, val + kParamCount, before); cleanSelection(); }
    void undo() { if (undoStack.empty()) return; redoStack.push_back(snapshot()); restoreSnap(undoStack.back()); undoStack.pop_back(); flash("Undo"); }
    void redo() { if (redoStack.empty()) return; undoStack.push_back(snapshot()); restoreSnap(redoStack.back()); redoStack.pop_back(); flash("Redo"); }
    void switchAB(int to) {
        if (to == abSlot) return;
        abStore[abSlot] = snapshot(); if (abStore[to].empty()) abStore[to] = snapshot();
        beginGesture(); applyAll(abStore[to].data()); endGesture(); cleanSelection();
        abSlot = to; flash(to == 0 ? "Setting A" : "Setting B");
    }
    void copyAB() { abStore[1 - abSlot] = snapshot(); flash(abSlot == 0 ? "Copied A to B" : "Copied B to A"); }
    void cleanSelection() { for (int b = 0; b < kNumBands; ++b) if (!used(b)) sel[b] = false; if (focus >= 0 && !used(focus)) focus = -1; }

    // ---- bands ------------------------------------------------------------------
    int newBand(float hz, float db) {
        int b = -1; for (int i = 0; i < kNumBands; ++i) if (!used(i)) { b = i; break; }
        if (b < 0) { flash("All 24 bands are in use"); return -1; }
        commit(bp(b, kBShape), kBell); commit(bp(b, kBFreq), clampf(hz, FMIN, FMAX)); commit(bp(b, kBGain), clampf(db, -30, 30));
        commit(bp(b, kBQ), 1.f); commit(bp(b, kBSlope), 2); commit(bp(b, kBPlace), 0); commit(bp(b, kBOn), 1); commit(bp(b, kBUsed), 1);
        return b;
    }
    void deleteSelected() {
        if (selCount() == 0 && primary() < 0) return;
        beginGesture();
        for (int b = 0; b < kNumBands; ++b) if ((sel[b] || b == focus) && used(b)) { commit(bp(b, kBUsed), 0); commit(bp(b, kBGain), 0); sel[b] = false; }
        endGesture(); focus = -1; repaint();
    }
    void selectOnly(int b) { for (bool& s : sel) s = false; if (b >= 0) sel[b] = true; focus = b; }

    // ---- formatting ---------------------------------------------------------
    void fmtVal(uint32_t p, float v, char* b, size_t n) const {
        int ne = 0;
        if (const char* const* l = enumLabels(p, ne)) { snprintf(b, n, "%s", l[std::max(0, std::min(ne - 1, (int)std::round(v)))]); return; }
        if (paramDef(p).hints & kHintBoolean) { snprintf(b, n, "%s", v > 0.5f ? "On" : "Off"); return; }
        const int f = fieldOf(p);
        if (f == kBFreq) { if (v >= 1000) snprintf(b, n, "%.2f kHz", v / 1000); else snprintf(b, n, v < 100 ? "%.1f Hz" : "%.0f Hz", v); return; }
        if (f == kBGain || p == kOutGain) { snprintf(b, n, "%+.2f dB", v); return; }
        if (f == kBQ) { snprintf(b, n, v < 10 ? "%.3f" : "%.1f", v); return; }
        snprintf(b, n, "%.2f", v);
    }
    void fmt(uint32_t p, char* b, size_t n) const { fmtVal(p, val[p], b, n); }
    bool parse(uint32_t /*p*/, const std::string& in, float& out) const {
        std::string s; for (char ch : in) if (!std::isspace((unsigned char)ch)) s += (char)std::tolower((unsigned char)ch);
        if (s.empty()) return false;
        char* end = nullptr; double x = std::strtod(s.c_str(), &end);
        if (end == s.c_str()) return false;
        std::string suf(end);
        if (suf == "k" || suf == "khz") x *= 1000;
        out = (float)x; return true;
    }
    uint32_t effParam(const Ctl& c) const { return c.field >= 0 ? (primary() >= 0 ? bp(primary(), c.field) : kParamCount) : c.param; }
    bool visible(const Ctl& c) const { return (c.field < 0 && c.type != DelButton) || primary() >= 0; }
    bool dimmed(const Ctl& c) const {
        if (c.param == kLinQuality && c.field < 0) return val[kProcMode] < 0.5f;
        if (c.field < 0 || primary() < 0) return false;
        const int sh = (int)(val[bp(primary(), kBShape)] + .5f);
        if (c.field == kBSlope) return !isCut(sh);
        if (c.field == kBGain) return isCut(sh);
        if (c.field != kBOn && val[bp(primary(), kBOn)] < 0.5f) return true;
        return false;
    }

    // ---- display mapping ------------------------------------------------------------
    float range() const { return rangeDb(val[kRange]); }
    static float xOfHz(float hz) { return DX + 30 + (DW - 44) * std::log(hz / FMIN) / std::log(FMAX / FMIN); }
    static float hzOfX(float x) { return FMIN * std::pow(FMAX / FMIN, (x - DX - 30) / (DW - 44)); }
    float yOfDb(float db) const { return DY + DH / 2 - db / range() * (DH / 2 - 22); }
    float dbOfY(float y) const { return (DY + DH / 2 - y) / (DH / 2 - 22) * range(); }
    // a band's dot: cuts sit at their resonance peak height above 0 dB
    void dotPos(int b, float& x, float& y) const {
        x = xOfHz(val[bp(b, kBFreq)]);
        const int sh = (int)(val[bp(b, kBShape)] + .5f);
        const float db = isCut(sh) ? 20.f * std::log10(val[bp(b, kBQ)] / 0.70710678f) : val[bp(b, kBGain)];
        y = yOfDb(clampf(db, -range(), range()));
    }
    static float slopeYdb(float dyDb, int shape) { return isCut(shape) ? std::pow(10.f, dyDb / 20.f) : dyDb; }

    // ---- drawing ------------------------------------------------------------
    void onNanoDisplay() override {
        const float s = uiScale();
        save(); scale(s, s);
        beginPath(); rect(0, 0, W, H); fillColor(C_BG); fill();
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        // logo: stacked translucent sheets
        for (int i = 0; i < 4; ++i) {
            beginPath(); const float o = i * 4.f;
            moveTo(16 + o, 26 + o); lineTo(30 + o, 20 + o); lineTo(38 + o, 26 + o); lineTo(24 + o, 32 + o); closePath();
            fillColor(withAlpha(C_ACC, 0.25f + 0.2f * i)); fill();
        }
        fontSize(22); textLetterSpacing(9); fillColor(C_TEXT); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); text(58, 34, "MICA", nullptr);
        fontSize(10); textLetterSpacing(2.5f); fillColor(C_DIM); text(162, 30, "EQUALIZER", nullptr);
        fontSize(7.5f); textLetterSpacing(1.8f); fillColor(C_FAINT); text(162, 42, "EPSILON AUDIO  1.0", nullptr);
        drawPresetBar();
        for (int k = 0; k < 2; ++k) {
            const float x = A_X + k * AB_W; const bool on = abSlot == k, hot = hoverHeader == 6 + k;
            beginPath(); roundedRect(x, PS_Y, AB_W - 0.5f, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.85f) : (hot ? C_PANEL_HI : C_PANEL)); fill(); strokeColor(on ? C_ACC : C_LINE); strokeWidth(1); stroke();
            fontSize(11); textLetterSpacing(0); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_BG : C_DIM); text(x + AB_W / 2, PS_Y + PS_H / 2 + 0.5f, k ? "B" : "A", nullptr);
        }
        drawHeaderButton(COPY_X, COPY_W, "COPY", false, hoverHeader == 8, false);
        drawHeaderButton(UNDO_X, UR_W, "UNDO", false, hoverHeader == 3, undoStack.empty());
        drawHeaderButton(REDO_X, UR_W, "REDO", false, hoverHeader == 4, redoStack.empty());
        drawHeaderButton(LEARN_X, LEARN_W, learnMode ? "LEARNING" : "LEARN", learnMode, hoverHeader == 1, false);
        drawHeaderButton(MIDI_X, MIDI_W, "MIDI", val[kMidiOn] > 0.5f, hoverHeader == 2, false);
        drawHeaderButton(HELP_X, HELP_W, "HELP", helpOpen, hoverHeader == 5, false);

        drawDisplay();
        panel(BX, PY, BW, PH); panel(GX, PY, GW, PH);
        drawCaption(GX + 140, PY + 17, "PROCESSING"); drawCaption(GX + 340, PY + 17, "QUALITY");
        drawCaption(GX + 140, PY + 69, "CHANNELS"); drawCaption(GX + 340, PY + 69, "ANALYZER"); drawCaption(GX + 140, PY + 121, "DISPLAY RANGE");
        if (primary() < 0) {
            fontSize(12); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_DIM);
            text(BX + BW / 2, PY + PH / 2 - 10, used0() ? "Click a band's dot to edit it" : "Double-click the display, or click on the curve, to add a band", nullptr);
            fontSize(10); fillColor(C_FAINT); text(BX + BW / 2, PY + PH / 2 + 14, "Up to 24 bands  ·  Shift-click or drag a box to select several", nullptr);
        }
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) if (visible(cs[i])) drawCtl(cs[i], i == active || i == hover);
        if (editing >= 0) drawEditBox();
        drawStatus();
        if (menuOpen) drawPresetMenu();
        if (saving) drawSaveBox();
        if (helpOpen) drawHelp();
        else if (tipShown && gTipsOn) drawTooltip();
        restore();
    }
    bool used0() const { for (int b = 0; b < kNumBands; ++b) if (used(b)) return true; return false; }
    void panel(float x, float y, float w, float h) { beginPath(); roundedRect(x, y, w, h, 6); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke(); }
    void drawCaption(float cx, float y, const char* t) { fontSize(7.5f); textLetterSpacing(1.8f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_FAINT); text(cx, y, t, nullptr); }
    void drawHeaderButton(float x, float w, const char* label, bool on, bool hot, bool disabled) {
        beginPath(); roundedRect(x, PS_Y, w, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.2f) : C_PANEL); fill();
        strokeColor(on ? C_ACC : (hot && !disabled ? C_DIM : C_LINE)); strokeWidth(1); stroke();
        fontSize(9.5f); textLetterSpacing(2.f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(disabled ? C_FAINT : (on ? C_TEXT : C_DIM));
        text(x + w / 2, PS_Y + PS_H / 2 + 0.5f, label, nullptr);
    }

    void drawDisplay() {
        beginPath(); roundedRect(DX, DY, DW, DH, 6); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        // grid
        const float grid[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        const char* gl[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
        beginPath();
        for (float g : grid) { const float x = xOfHz(g); moveTo(x, DY + 6); lineTo(x, DY + DH - 18); }
        const float r = range(), step = r <= 6 ? 2.f : (r <= 12 ? 3.f : 10.f);
        for (float db = -r; db <= r + 0.01f; db += step) { const float y = yOfDb(db); moveTo(DX + 26, y); lineTo(DX + DW - 8, y); }
        strokeColor(Color(255, 255, 255, 8 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(DX + 26, yOfDb(0)); lineTo(DX + DW - 8, yOfDb(0)); strokeColor(Color(255, 255, 255, 22 / 255.f)); stroke();
        fontSize(8.5f); textLetterSpacing(0.8f); fillColor(C_FAINT); textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        for (int i = 0; i < 10; ++i) text(xOfHz(grid[i]), DY + DH - 4, gl[i], nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        for (float db = -r; db <= r + 0.01f; db += step) { char b[8]; snprintf(b, sizeof b, "%+.0f", db); text(DX + 23, yOfDb(db), std::fabs(db) < 0.01f ? "0" : b, nullptr); }

        // analyser (own dB scale: 0 dBFS at the top, -90 at the bottom, 3 dB/oct tilt around 1 kHz)
        if (val[kAnalyzer] > 0.5f && anaValid) {
            const float sr = sampleRateUI;
            beginPath(); moveTo(xOfHz(FMIN), DY + DH - 18);
            const int cols = 300;
            for (int i = 0; i <= cols; ++i) {
                const float f0 = hzOfX(DX + 30 + (DW - 44) * (float)i / cols), f1 = hzOfX(DX + 30 + (DW - 44) * (float)(i + 1) / cols);
                int k0 = (int)(f0 / sr * ANA_N), k1 = std::max(k0 + 1, (int)(f1 / sr * ANA_N));
                k0 = std::max(1, std::min(k0, ANA_N / 2 - 1)); k1 = std::max(k0 + 1, std::min(k1, ANA_N / 2));
                float m = -120; for (int k = k0; k < k1; ++k) m = std::max(m, spec[k]);
                const float tilt = 3.f * std::log2(std::max(f0, 1.f) / 1000.f);
                const float y = clampf(DY + 8 + (DH - 26) * (-(m + tilt) / 90.f), DY + 8, DY + DH - 18);
                lineTo(xOfHz(f0), y);
            }
            lineTo(xOfHz(FMAX), DY + DH - 18); closePath();
            fillPaint(linearGradient(0, DY, 0, DY + DH, withAlpha(C_ANA, 0.45f), withAlpha(C_ANA, 0.08f))); fill();
        }

        // individual band curves (selected ones filled)
        const int N = 360;
        scissor(DX + 1, DY + 1, DW - 2, DH - 19);   // curves run off the edge instead of flattening along it
        for (int b = 0; b < kNumBands; ++b) {
            if (!used(b)) continue;
            const bool isSel = sel[b] || b == focus, on = val[bp(b, kBOn)] > 0.5f;
            const Color col = bandColor(b);
            const int sh = (int)(val[bp(b, kBShape)] + .5f), sl = (int)(val[bp(b, kBSlope)] + .5f);
            beginPath();
            for (int i = 0; i <= N; ++i) {
                const float x = DX + 30 + (DW - 44) * i / N, f = hzOfX(x);
                const float db = 20 * std::log10(std::max(std::abs(bandResponse(sh, sl, val[bp(b, kBFreq)], val[bp(b, kBGain)], val[bp(b, kBQ)], f)), 1e-5f));
                const float y = clampf(yOfDb(db), DY - 40, DY + DH + 40);
                if (!i) moveTo(x, y); else lineTo(x, y);
            }
            if (isSel) {
                lineTo(DX + DW - 14, yOfDb(0)); lineTo(DX + 30, yOfDb(0)); closePath();
                fillColor(withAlpha(col, on ? 0.16f : 0.06f)); fill();
                beginPath();
                for (int i = 0; i <= N; ++i) {
                    const float x = DX + 30 + (DW - 44) * i / N, f = hzOfX(x);
                    const float db = 20 * std::log10(std::max(std::abs(bandResponse(sh, sl, val[bp(b, kBFreq)], val[bp(b, kBGain)], val[bp(b, kBQ)], f)), 1e-5f));
                    const float y = clampf(yOfDb(db), DY - 40, DY + DH + 40);
                    if (!i) moveTo(x, y); else lineTo(x, y);
                }
            }
            strokeColor(withAlpha(col, on ? (isSel ? 0.9f : 0.35f) : 0.15f)); strokeWidth(isSel ? 1.5f : 1.f); stroke();
        }
        // combined curve(s): one line, or two when left/mid and right/side differ
        bool split = false;
        for (int b = 0; b < kNumBands; ++b) if (used(b) && val[bp(b, kBOn)] > 0.5f && val[bp(b, kBPlace)] > 0.5f) split = true;
        for (int c = split ? 1 : 0; c >= 0; --c) {
            beginPath();
            for (int i = 0; i <= N; ++i) {
                const float x = DX + 30 + (DW - 44) * i / N;
                const float y = clampf(yOfDb(20 * std::log10(std::max(std::abs(respAt(val, hzOfX(x), c)), 1e-5f))), DY - 40, DY + DH + 40);
                if (!i) moveTo(x, y); else lineTo(x, y);
            }
            strokeColor(split ? (c == 0 ? Color(0xf0, 0xd8, 0x90) : Color(0x9c, 0xc8, 0xf8)) : C_TEXT); strokeWidth(2.2f); stroke();
        }
        if (split) {
            const bool ms = val[kChannelMode] > 0.5f;
            fontSize(8.5f); textLetterSpacing(1.4f); textAlign(ALIGN_LEFT | ALIGN_TOP);
            fillColor(Color(0xf0, 0xd8, 0x90)); text(DX + 34, DY + 10, ms ? "MID" : "LEFT", nullptr);
            fillColor(Color(0x9c, 0xc8, 0xf8)); text(DX + 74, DY + 10, ms ? "SIDE" : "RIGHT", nullptr);
        }
        resetScissor();
        // dots
        for (int b = 0; b < kNumBands; ++b) {
            if (!used(b)) continue;
            float x, y; dotPos(b, x, y);
            const bool isSel = sel[b] || b == focus, on = val[bp(b, kBOn)] > 0.5f, hot = hoverDot == b;
            const Color col = bandColor(b);
            if (isSel || hot) { beginPath(); circle(x, y, 16); fillColor(withAlpha(col, 0.18f)); fill(); }
            beginPath(); circle(x, y, 10); fillColor(on ? col : C_PANEL_HI); fill();
            if (isSel) { strokeColor(C_TEXT); strokeWidth(2); stroke(); } else if (!on) { strokeColor(col); strokeWidth(1.2f); stroke(); }
            char t[4]; snprintf(t, sizeof t, "%d", b + 1);
            fontSize(9.5f); textLetterSpacing(0); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_BG : col); text(x, y + 0.5f, t, nullptr);
        }
        // rubber band selection
        if (boxSel) {
            beginPath(); rect(std::min(boxX0, boxX1), std::min(boxY0, boxY1), std::fabs(boxX1 - boxX0), std::fabs(boxY1 - boxY0));
            fillColor(withAlpha(C_ACC, 0.08f)); fill(); strokeColor(withAlpha(C_ACC, 0.6f)); strokeWidth(1); stroke();
        }
        // readout for the dot under the mouse / being dragged
        const int rb = dragDot >= 0 ? dragDot : hoverDot;
        if (rb >= 0 && used(rb)) {
            float x, y; dotPos(rb, x, y);
            char f[24], g[24], q[24]; fmt(bp(rb, kBFreq), f, sizeof f); fmt(bp(rb, kBGain), g, sizeof g); fmt(bp(rb, kBQ), q, sizeof q);
            int ne; const char* shape = enumLabels(bp(rb, kBShape), ne)[(int)(val[bp(rb, kBShape)] + .5f)];
            char line[96];
            if (isCut((int)(val[bp(rb, kBShape)] + .5f))) snprintf(line, sizeof line, "%d  %s   %s   Q %s", rb + 1, shape, f, q);
            else snprintf(line, sizeof line, "%d  %s   %s   %s   Q %s", rb + 1, shape, f, g, q);
            fontSize(10); textLetterSpacing(0.4f);
            Rectangle<float> tb; textBounds(0, 0, line, nullptr, tb);
            const float w = tb.getWidth() + 16, bx = clampf(x - w / 2, DX + 6, DX + DW - w - 6), by = y > DY + 60 ? y - 42 : y + 20;
            beginPath(); roundedRect(bx, by, w, 20, 4); fillColor(Color(0x1d, 0x21, 0x25, 0.95f)); fill(); strokeColor(withAlpha(bandColor(rb), 0.8f)); strokeWidth(1); stroke();
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(bx + 8, by + 10.5f, line, nullptr);
        }
    }

    void drawKnobBody(float x, float y, float r, float n, bool bipolar, bool hot, Color col, bool dim) {
        const float a0 = 0.75f * 3.14159265f, a1 = 2.25f * 3.14159265f, ang = a0 + (a1 - a0) * n;
        beginPath(); arc(x, y, r, a0, a1, NanoVG::CW); strokeColor(C_LINE); strokeWidth(r > 24 ? 5.f : 3.5f); lineCap(ROUND); stroke();
        const float from = bipolar ? (a0 + a1) / 2 : a0;
        if (std::fabs(ang - from) > 0.001f) { beginPath(); arc(x, y, r, std::min(from, ang), std::max(from, ang), NanoVG::CW); strokeColor(dim ? C_FAINT : col); stroke(); }
        const float cr = r - (r > 24 ? 9 : 7);
        beginPath(); circle(x, y, cr);
        fillPaint(radialGradient(x - cr * 0.3f, y - cr * 0.4f, 0, cr * 1.4f, hot ? Color(0x3a, 0x40, 0x46) : C_PANEL_HI, Color(0x13, 0x15, 0x17)));
        fill(); strokeColor(Color(255, 255, 255, 18 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(x + std::cos(ang) * cr * 0.35f, y + std::sin(ang) * cr * 0.35f); lineTo(x + std::cos(ang) * cr * 0.9f, y + std::sin(ang) * cr * 0.9f);
        strokeColor(dim ? C_DIM : C_TEXT); strokeWidth(r > 24 ? 2.5f : 2.f); lineCap(ROUND); stroke();
    }
    const char* const* segNames(const Ctl& c, int& n) const {
        static const char* const shape[] = { "BELL", "LO SHELF", "HI SHELF", "LO CUT", "HI CUT" };
        static const char* const slope[] = { "6", "12", "24", "48" };
        static const char* const placeLR[] = { "STEREO", "LEFT", "RIGHT" };
        static const char* const placeMS[] = { "STEREO", "MID", "SIDE" };
        static const char* const mode[] = { "ZERO LATENCY", "LINEAR PHASE" };
        static const char* const lq[] = { "LOW", "MED", "HIGH" };
        static const char* const chm[] = { "STEREO", "MID / SIDE" };
        static const char* const ana[] = { "OFF", "PRE", "POST" };
        static const char* const rng[] = { "± 6 dB", "± 12 dB", "± 30 dB" };
        switch (c.field) {
        case kBShape: n = 5; return shape;
        case kBSlope: n = 4; return slope;
        case kBPlace: n = 3; return val[kChannelMode] > 0.5f ? placeMS : placeLR;
        default: break;
        }
        switch (c.param) {
        case kProcMode: n = 2; return mode;
        case kLinQuality: n = 3; return lq;
        case kChannelMode: n = 2; return chm;
        case kAnalyzer: n = 3; return ana;
        case kRange: n = 3; return rng;
        default: n = 0; return nullptr;
        }
    }
    void drawCtl(const Ctl& c, bool hot) {
        const uint32_t p = effParam(c); if (p >= kParamCount && c.type != DelButton) return;
        const bool dim = dimmed(c); char buf[48];
        const Color col = c.field >= 0 && primary() >= 0 ? bandColor(primary()) : C_ACC;
        switch (c.type) {
        case Knob: {
            const bool bip = c.field == kBGain || p == kOutGain;
            drawKnobBody(c.x, c.y, c.r, toNorm(p, val[p]), bip, hot, col, dim);
            fontSize(8.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_TOP);
            if (c.r >= 28) {
                fillColor(dim ? C_FAINT : C_DIM); text(c.x, c.y + c.r + 6, c.label, nullptr);
                fmt(p, buf, sizeof buf); fontSize(11); textLetterSpacing(0.4f); fillColor(dim ? C_FAINT : (hot ? col : C_TEXT)); text(c.x, c.y + c.r + 19, buf, nullptr);
            } else if (hot && editing < 0 && !learnMode) { fmt(p, buf, sizeof buf); fillColor(C_ACC); text(c.x, c.y + c.r + 6, buf, nullptr); }
            else { fillColor(C_DIM); text(c.x, c.y + c.r + 6, c.label, nullptr); }
            break; }
        case Toggle: {
            const bool on = val[p] > 0.5f;
            const float w = 34, h = 18, x = c.x - w / 2, y = c.y - h / 2;
            beginPath(); roundedRect(x, y, w, h, h / 2); fillColor(on ? withAlpha(col, 0.3f) : C_PANEL_HI); fill(); strokeColor(on ? col : C_LINE); strokeWidth(1); stroke();
            beginPath(); circle(on ? x + w - h / 2 : x + h / 2, c.y, 6); fillColor(on ? col : C_DIM); fill();
            const int ns = selCount(); char t[24];
            if (ns > 1) snprintf(t, sizeof t, "%d BANDS", ns); else snprintf(t, sizeof t, "BAND %d", primary() + 1);
            fontSize(11); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM); text(c.x + w / 2 + 10, c.y + (ns > 1 ? -4.f : 0.5f), t, nullptr);
            if (ns > 1) { fontSize(7); textLetterSpacing(1.4f); fillColor(C_ACC); text(c.x + w / 2 + 10, c.y + 9, "EDITS APPLY TO ALL", nullptr); }
            break; }
        case Segment: {
            int n = 0; const char* const* names = segNames(c, n);
            const int cur = (int)(val[p] + .5f); const float sw = c.r / n, x0 = c.x - c.r / 2;
            for (int i = 0; i < n; ++i) {
                const float x = x0 + sw * i;
                beginPath(); roundedRect(x + 1.5f, c.y - SEG_H / 2, sw - 3, SEG_H, 5); fillColor(i == cur ? (dim ? C_FAINT : col) : C_PANEL_HI); fill();
                fontSize(9); textLetterSpacing(1.4f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(i == cur ? C_BG : (dim ? C_FAINT : C_DIM)); text(x + sw / 2, c.y + 0.5f, names[i], nullptr);
            }
            break; }
        case DelButton: {
            beginPath(); roundedRect(c.x - 14, c.y - 12, 28, 24, 5); fillColor(hot ? withAlpha(Color(0xe0, 0x60, 0x50), 0.3f) : C_PANEL_HI); fill(); strokeColor(hot ? Color(0xe0, 0x60, 0x50) : C_LINE); strokeWidth(1); stroke();
            beginPath(); moveTo(c.x - 5, c.y - 5); lineTo(c.x + 5, c.y + 5); moveTo(c.x + 5, c.y - 5); lineTo(c.x - 5, c.y + 5); strokeColor(hot ? C_TEXT : C_DIM); strokeWidth(1.6f); stroke();
            break; }
        }
        if (learnMode && c.type != DelButton) {
            const int cc = paramCC[p]; const bool waiting = learnWaiting == (int)p;
            if (cc >= 0 || waiting) {
                const float by = c.type == Knob ? c.y - c.r - 12 : c.y - SEG_H / 2 - 8;
                snprintf(buf, sizeof buf, waiting ? "?" : "CC%d", cc);
                fontSize(8.5f); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                const float bw = waiting ? 16 : 34;
                beginPath(); roundedRect(c.x - bw / 2, by - 7, bw, 14, 3); fillColor(waiting ? C_ACC : C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fillColor(waiting ? C_BG : C_ACC); text(c.x, by + 0.5f, buf, nullptr);
            }
        }
    }
    void drawStatus() {
        std::string msg;
        if (statusFlash > 0) msg = statusText;
        else if (learnMode) msg = learnWaiting >= 0 ? std::string("Move a MIDI controller to assign: ") + paramDef(learnWaiting).name : "MIDI Learn: click a control, then move a controller.  Right-click a control to unassign.";
        if (!msg.empty()) { fontSize(10); textLetterSpacing(0.8f); textAlign(ALIGN_RIGHT | ALIGN_TOP); fillColor(C_ACC); text(DX + DW - 12, DY + 10, msg.c_str(), nullptr); }
        if (val[kProcMode] > 0.5f) {
            const double sr = getSampleRate() > 0 ? getSampleRate() : 48000.0;
            char b[48]; snprintf(b, sizeof b, "LINEAR PHASE  ·  %d ms LATENCY", (int)std::round((2048 << (int)(val[kLinQuality] + .5f)) * 1.5 * 1000.0 / sr));
            fontSize(8); textLetterSpacing(1.5f); textAlign(ALIGN_RIGHT | ALIGN_TOP); fillColor(C_DIM); text(DX + DW - 12, DY + (msg.empty() ? 12 : 28), b, nullptr);
        }
        if (learnMode && learnWaiting < 0) { textAlign(ALIGN_RIGHT | ALIGN_BOTTOM); fillColor(hoverClear ? C_TEXT : C_DIM); text(DX + DW - 12, DY + DH - 22, "CLEAR ALL ASSIGNMENTS", nullptr); }
        if (statusFlash > 0) { --statusFlash; repaint(); }
    }
    void flash(const std::string& s) { statusText = s; statusFlash = 60; repaint(); }
    void drawEditBox() {
        const Ctl& c = controls()[editing];
        const float w = 90, h = 22, x = c.x - w / 2, y = c.y + c.r + 4;
        beginPath(); roundedRect(x, y, w, h, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(12); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(c.x, y + h / 2 + 0.5f, (editBuf + "|").c_str(), nullptr);
    }

    // ---- presets ------------------------------------------------------------------
    bool matchesValues(const float* v) const {
        for (uint32_t i = 0; i < kParamCount; ++i) {
            if ((paramDef(i).hints & kHintOutput) || i == kAnalyzer || i == kRange) continue;
            if (std::fabs(val[i] - v[i]) > 1e-4f * std::max(1.f, std::fabs(v[i]))) return false;
        }
        return true;
    }
    void applyValues(const float* v) {
        float w[kParamCount]; std::copy(v, v + kParamCount, w); w[kAnalyzer] = val[kAnalyzer]; w[kRange] = val[kRange];   // keep the view settings
        beginGesture(); applyAll(w); endGesture(); for (bool& s : sel) s = false; focus = -1;
    }
    void applyFactory(int idx) { idx = (idx % (int)kNumPresets + (int)kNumPresets) % (int)kNumPresets; applyValues(presets()[idx].v); curFactory = idx; curUser.clear(); repaint(); }
    void applyUser(const std::string& name) { float v[kParamCount]; if (!loadUserPreset(name, v)) return; applyValues(v); std::copy(v, v + kParamCount, userVals); curUser = name; curFactory = -1; repaint(); }
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
    struct MRect { float x, y, w, h; int kind; int idx; };
    void menuLayout(float& mx, float& my, float& mw, float& mh, std::vector<MRect>& items) const {
        const int cols = kNumCategories + 1;
        mw = PM_COLW * cols + 2 * PM_PAD; mx = std::min(W - mw - 8, std::max(8.f, PS_X + PS_W / 2 - mw / 2)); my = PS_Y + PS_H + 4;
        items.clear();
        const float top = my + PM_PAD + PM_ROW + 6 + PM_HEAD;
        int rows[kNumCategories + 1] = {}; int maxRows = 1;
        for (uint32_t i = 0; i < kNumPresets; ++i) {
            const char* cat = presets()[i].category;
            if (std::strcmp(cat, "Init") == 0) { items.push_back({ mx + PM_PAD, my + PM_PAD, PM_COLW - 4, PM_ROW, 0, (int)i }); continue; }
            for (int c = 0; c < kNumCategories; ++c)
                if (std::strcmp(cat, kCategories[c]) == 0) { items.push_back({ mx + PM_PAD + c * PM_COLW, top + rows[c] * PM_ROW, PM_COLW - 4, PM_ROW, 0, (int)i }); maxRows = std::max(maxRows, ++rows[c]); }
        }
        for (int u = 0; u < (int)userPresets.size() && u < 16; ++u) items.push_back({ mx + PM_PAD + kNumCategories * PM_COLW, top + u * PM_ROW, PM_COLW - 4, PM_ROW, 1, u });
        maxRows = std::max(maxRows, std::min(16, (int)userPresets.size()));
        items.push_back({ mx + mw - PM_PAD - 150, my + PM_PAD, 150, PM_ROW, 2, 0 });
        mh = PM_PAD + PM_ROW + 6 + PM_HEAD + maxRows * PM_ROW + PM_PAD;
    }
    void drawPresetMenu() {
        float mx, my, mw, mh; std::vector<MRect> items; menuLayout(mx, my, mw, mh, items);
        beginPath(); rect(mx - 12, my - 4, mw + 24, mh + 20); fillPaint(boxGradient(mx, my + 2, mw, mh, 6, 14, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(mx, my, mw, mh, 6); fillColor(C_PANEL_HI); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(mx + PM_PAD, my + PM_PAD + PM_ROW + 3); lineTo(mx + mw - PM_PAD, my + PM_PAD + PM_ROW + 3); strokeColor(C_LINE); stroke();
        fontSize(8.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        for (int c = 0; c <= kNumCategories; ++c) {
            std::string up = c < kNumCategories ? kCategories[c] : "User"; for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
            fillColor(c < kNumCategories ? C_ACC : Color(0xf0, 0xd8, 0x90)); text(mx + PM_PAD + c * PM_COLW + 12, my + PM_PAD + PM_ROW + 6 + PM_HEAD / 2, up.c_str(), nullptr);
        }
        if (userPresets.empty()) { fontSize(10); textLetterSpacing(0.4f); fillColor(C_FAINT); text(mx + PM_PAD + kNumCategories * PM_COLW + 12, my + PM_PAD + PM_ROW + 6 + PM_HEAD + PM_ROW / 2, "(none saved yet)", nullptr); }
        for (int i = 0; i < (int)items.size(); ++i) {
            const MRect& r = items[i]; const bool hot = i == menuHover;
            if (r.kind == 2) {
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(hot ? withAlpha(C_ACC, 0.25f) : C_PANEL); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fontSize(9.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(r.x + r.w / 2, r.y + r.h / 2 + 0.5f, "SAVE CURRENT AS...", nullptr);
                continue;
            }
            const bool isCur = r.kind == 0 ? (curUser.empty() && r.idx == curFactory) : (curUser == userPresets[r.idx]);
            const bool del = r.kind == 1 && pendingDelete == r.idx;
            if (hot || del) { beginPath(); roundedRect(r.x, r.y, r.w, r.h, 4); fillColor(del ? Color(0xd0, 0x50, 0x40, 0.35f) : withAlpha(C_ACC, 0.18f)); fill(); }
            if (isCur) { beginPath(); circle(r.x + 6, r.y + r.h / 2, 2.5f); fillColor(C_ACC); fill(); }
            fontSize(11); textLetterSpacing(0.4f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(hot || isCur || del ? C_TEXT : C_DIM);
            const std::string label = del ? "Delete? click again" : (r.kind == 0 ? std::string(presets()[r.idx].name) : userPresets[r.idx]);
            text(r.x + 12, r.y + r.h / 2 + 0.5f, label.c_str(), nullptr);
        }
    }
    int menuHit(float x, float y) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it);
        for (int i = 0; i < (int)it.size(); ++i) if (x >= it[i].x && x < it[i].x + it[i].w && y >= it[i].y && y < it[i].y + it[i].h) return i;
        return -1; }
    MRect menuItem(int i) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it); return it[i]; }
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

    // ---- analyser + tips (idle) ---------------------------------------------------------------
    void uiIdle() override {
        const double t = nowSec();
        if (val[kAnalyzer] > 0.5f && t - lastAna > 0.033) {
            lastAna = t;
            if (MicaPlugin* const dsp = (MicaPlugin*)getPluginInstancePointer()) {
                const Engine& e = dsp->engine;
                const uint32_t w = e.anaWrite.load(std::memory_order_acquire);
                if (w != lastWrite) {
                    lastWrite = w; sampleRateUI = e.sampleRate();
                    for (int i = 0; i < ANA_N; ++i) {
                        const float hann = 0.5f - 0.5f * std::cos(2 * kPi * i / (ANA_N - 1));
                        buf[i] = cpx(e.ana[(w - ANA_N + i) & (Engine::kAnaSize - 1)] * hann, 0.f);
                    }
                    fft.run(buf.data(), false);
                    const float norm = 2.f / (ANA_N * 0.5f);
                    for (int k = 1; k < ANA_N / 2; ++k) {
                        const float db = 20.f * std::log10(std::max(std::abs(buf[k]) * norm, 1e-7f));
                        spec[k] = db > spec[k] ? db : std::max(db, spec[k] - 1.8f);       // fast rise, ~55 dB/s fall
                    }
                    anaValid = true; repaint();
                } else if (anaValid) {     // no audio arriving: let the display fall
                    for (float& s : spec) s = std::max(-120.f, s - 1.8f);
                    repaint();
                }
            }
        }
        if (gTipsOn && !tipShown && tipKey >= 0 && tipKey != tipSuppressed && active < 0 && dragDot < 0 && !boxSel && !menuOpen && !saving && !helpOpen && editing < 0
            && t - tipSince > 0.6) { tipShown = true; repaint(); }
    }
    bool tipContent(std::string& title, std::string& value, std::string& body) const {
        if (tipKey < 0) return false;
        if (tipKey < 1000) {
            const Ctl& c = controls()[tipKey];
            if (c.type == DelButton) { title = targetTitle(kTipDelete); value.clear(); body = targetTip(kTipDelete); return true; }
            const uint32_t p = effParam(c); if (p >= kParamCount) return false;
            char b[40]; fmt(p, b, sizeof b);
            title = paramDef(p).name; value = b; body = paramTip(p);
            if (c.type == Knob) body += "\n\nDrag the middle up/down or grab the rim and turn. Double-click to type a value, Cmd/Ctrl-click to reset.";
            if (paramCC[p] >= 0) { char cc[32]; snprintf(cc, sizeof cc, "   ·   CC%d", paramCC[p]); value += cc; }
            return true;
        }
        int t = -1;
        if (tipKey >= 5000) t = kTipDisplay;
        else if (tipKey >= 3000) t = kTipDot;
        else if (tipKey >= 2000) t = kTipPrev + (tipKey - 2000);
        else { const int hh = tipKey - 1000;
            t = hh == 1 ? kTipLearn : hh == 2 ? kTipMidi : hh == 3 ? kTipUndo : hh == 4 ? kTipRedo : hh == 5 ? kTipHelp : hh == 6 ? kTipA : hh == 7 ? kTipB : kTipCopy; }
        title = targetTitle(t); value.clear(); body = targetTip(t);
        if (t == kTipDot) { char b[32]; snprintf(b, sizeof b, "Band %d", tipKey - 3000 + 1); title = b; }
        if (t == kTipUndo || t == kTipRedo) { const size_t n = t == kTipUndo ? undoStack.size() : redoStack.size(); char b[32]; snprintf(b, sizeof b, "%d step%s", (int)n, n == 1 ? "" : "s"); value = b; }
        return true;
    }
    void drawTooltip() {
        std::string title, value, body;
        if (!tipContent(title, value, body)) return;
        const float w = 320, pad = 12;
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f);
        float bb[4]; textBoxBounds(0, 0, w - 2 * pad, body.c_str(), nullptr, bb);
        const float h = pad + 16 + 8 + (bb[3] - bb[1]) + pad;
        float x = tipX + 16, y = tipY + 20;
        if (x + w > W - 8) x = tipX - w - 12;
        if (y + h > H - 8) y = tipY - h - 12;
        x = clampf(x, 8, W - w - 8); y = clampf(y, 8, H - h - 8);
        beginPath(); rect(x - 10, y - 4, w + 20, h + 18); fillPaint(boxGradient(x, y + 3, w, h, 6, 14, Color(0, 0, 0, 0.55f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(x, y, w, h, 6); fillColor(Color(0x1d, 0x21, 0x25)); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_TEXT);
        std::string up = title; for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
        text(x + pad, y + pad + 1, up.c_str(), nullptr);
        if (!value.empty()) { fontSize(10.5f); textLetterSpacing(0.4f); textAlign(ALIGN_RIGHT | ALIGN_TOP); fillColor(C_ACC); text(x + w - pad, y + pad + 1, value.c_str(), nullptr); }
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM);
        textBox(x + pad, y + pad + 24, w - 2 * pad, body.c_str(), nullptr);
        textLineHeight(1.f);
    }

    // ---- help window ---------------------------------------------------------------
    static constexpr float HX = 170, HY = 80, HW = 760, HH = 520, TAB_W = 180, TAB_H = 34;
    int helpHit(float x, float y) const {
        if (x < HX || x > HX + HW || y < HY || y > HY + HH) return -2;
        for (int i = 0; i < kNumHelpPages; ++i) if (x >= HX + 12 && x < HX + 12 + TAB_W && y >= HY + 56 + i * TAB_H && y < HY + 56 + (i + 1) * TAB_H) return i;
        const float by = HY + HH - 44;
        if (y >= by && y < by + 28) {
            if (x >= HX + 12 && x < HX + 12 + TAB_W) return 100;
            if (x >= HX + HW - 330 && x < HX + HW - 140) return 101;
            if (x >= HX + HW - 124 && x < HX + HW - 16) return 102;
        }
        if (x >= HX + HW - 40 && y < HY + 40) return 102;
        return -1;
    }
    void helpClick(float x, float y) {
        const int h = helpHit(x, y);
        if (h >= 0 && h < kNumHelpPages) helpPage = h;
        else if (h == 100) gTipsOn = !gTipsOn;
        else if (h == 101) openURL(kManualURL);
        else if (h == 102 || h == -2) helpOpen = false;
        repaint();
    }
    void drawHelp() {
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.55f)); fill();
        beginPath(); roundedRect(HX, HY, HW, HH, 10); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(13); textLetterSpacing(4.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(HX + 22, HY + 28, "MICA HELP", nullptr);
        const float cx = HX + HW - 24, cy = HY + 26;
        beginPath(); moveTo(cx - 6, cy - 6); lineTo(cx + 6, cy + 6); moveTo(cx + 6, cy - 6); lineTo(cx - 6, cy + 6); strokeColor(helpHover == 102 ? C_TEXT : C_DIM); strokeWidth(1.6f); stroke();
        for (int i = 0; i < kNumHelpPages; ++i) {
            const float ty = HY + 56 + i * TAB_H;
            if (i == helpPage || i == helpHover) { beginPath(); roundedRect(HX + 12, ty + 2, TAB_W, TAB_H - 4, 5); fillColor(i == helpPage ? withAlpha(C_ACC, 0.18f) : C_PANEL_HI); fill(); }
            if (i == helpPage) { beginPath(); roundedRect(HX + 12, ty + 8, 3, TAB_H - 16, 1.5f); fillColor(C_ACC); fill(); }
            fontSize(11.5f); textLetterSpacing(0.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(i == helpPage ? C_TEXT : C_DIM);
            text(HX + 28, ty + TAB_H / 2 + 0.5f, kHelpPages[i].title, nullptr);
        }
        beginPath(); moveTo(HX + TAB_W + 26, HY + 56); lineTo(HX + TAB_W + 26, HY + HH - 56); strokeColor(C_LINE); strokeWidth(1); stroke();
        const float bx = HX + TAB_W + 46, bw = HW - TAB_W - 70;
        fontSize(15); textLetterSpacing(0.8f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_ACC); text(bx, HY + 60, kHelpPages[helpPage].title, nullptr);
        fontSize(12); textLetterSpacing(0.2f); textLineHeight(1.38f); fillColor(Color(0xc6, 0xcc, 0xd0));
        textBox(bx, HY + 92, bw, kHelpPages[helpPage].body, nullptr);
        textLineHeight(1.f);
        const float by = HY + HH - 44;
        beginPath(); moveTo(HX + 12, by - 10); lineTo(HX + HW - 12, by - 10); strokeColor(C_LINE); stroke();
        const float tw = 34, th = 18, tx = HX + 16, tyy = by + 14 - th / 2;
        beginPath(); roundedRect(tx, tyy, tw, th, th / 2); fillColor(gTipsOn ? withAlpha(C_ACC, 0.25f) : C_PANEL_HI); fill(); strokeColor(gTipsOn ? C_ACC : C_LINE); stroke();
        beginPath(); circle(gTipsOn ? tx + tw - th / 2 : tx + th / 2, by + 14, 6); fillColor(gTipsOn ? C_ACC : C_DIM); fill();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(helpHover == 100 ? C_TEXT : C_DIM); text(tx + tw + 10, by + 14.5f, "HOVER TIPS", nullptr);
        auto btn = [&](float x0, float w0, const char* label, bool hot, bool primaryBtn) {
            beginPath(); roundedRect(x0, by, w0, 28, 5); fillColor(primaryBtn ? (hot ? withAlpha(C_ACC, 0.35f) : withAlpha(C_ACC, 0.18f)) : (hot ? C_PANEL_HI : C_PANEL)); fill();
            strokeColor(primaryBtn ? C_ACC : C_LINE); strokeWidth(1); stroke();
            fontSize(9.5f); textLetterSpacing(1.8f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(x0 + w0 / 2, by + 14.5f, label, nullptr);
        };
        btn(HX + HW - 330, 190, "FULL MANUAL (WEB)", helpHover == 101, false);
        btn(HX + HW - 124, 108, "CLOSE", helpHover == 102, true);
    }

    // ---- interaction --------------------------------------------------------------
    int hit(float x, float y, int* seg = nullptr) const {
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) {
            const Ctl& c = cs[i];
            if (!visible(c)) continue;
            switch (c.type) {
            case Knob: { const float rr = c.r + 6; if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= rr * rr) return i; break; }
            case Toggle: if (x >= c.x - 20 && x <= c.x + 90 && std::fabs(y - c.y) <= 12) return i; break;
            case DelButton: if (std::fabs(x - c.x) <= 14 && std::fabs(y - c.y) <= 12) return i; break;
            case Segment: { int n = 0; segNames(c, n); const float x0 = c.x - c.r / 2;
                if (x >= x0 && x < x0 + c.r && std::fabs(y - c.y) <= SEG_H / 2 + 1) { if (seg) *seg = (int)((x - x0) / (c.r / n)); return i; } break; }
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
    int presetBarHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H || x < PS_X || x > PS_X + PS_W) return -1;
        return x < PS_X + PS_ARROW ? 0 : (x > PS_X + PS_W - PS_ARROW ? 2 : 1);
    }
    bool clearHit(float x, float y) const { return learnMode && learnWaiting < 0 && inRect(x, y, DX + DW - 200, DY + DH - 36, 190, 18); }
    bool inDisplay(float x, float y) const { return inRect(x, y, DX + 26, DY, DW - 30, DH - 16); }
    int dotHit(float x, float y) const {
        int best = -1; float bd = 15 * 15;
        for (int b = 0; b < kNumBands; ++b) { if (!used(b)) continue; float dx, dy; dotPos(b, dx, dy); const float d = (x - dx) * (x - dx) + (y - dy) * (y - dy); if (d < bd) { bd = d; best = b; } }
        return best;
    }
    bool onCurve(float x, float y) const {
        const float hz = hzOfX(x);
        for (int c = 0; c < 2; ++c) if (std::fabs(yOfDb(20 * std::log10(std::max(std::abs(respAt(val, hz, c)), 1e-5f))) - y) < 7) return true;
        return false;
    }

    void setLearn(bool on) { learnMode = on; learnWaiting = -1; if (!on) setState("cmd", "cancel"); repaint(); }
    void endEdit(bool apply) {
        if (editing < 0) return;
        const uint32_t p = effParam(controls()[editing]); float v;
        if (apply && p < kParamCount && parse(p, editBuf, v)) { beginGesture(); applyToSelection(p, v, val[p]); endGesture(); }
        editing = -1; editBuf.clear(); repaint();
    }
    // set a band field on the focused band; other selected bands follow relatively (freq/Q by ratio, gain by offset) or take the same choice
    void applyToSelection(uint32_t p, float v, float old) {
        const int f = fieldOf(p);
        setValue(p, v);
        if (f < 0) return;
        const float nv = val[p];
        for (int b = 0; b < kNumBands; ++b) {
            if (!sel[b] || b == bandOf(p) || !used(b)) continue;
            const uint32_t q = bp(b, f);
            if (f == kBFreq || f == kBQ) setValue(q, val[q] * nv / std::max(old, 1e-6f));
            else if (f == kBGain) setValue(q, val[q] + nv - old);
            else setValue(q, nv);
        }
    }
    void editSelection(bool begin) {
        for (int b = 0; b < kNumBands; ++b) if ((sel[b] || b == focus) && used(b)) for (int f = 0; f < kBandFields; ++f) { if (begin) editParameter(bp(b, f), true); else editParameter(bp(b, f), false); }
    }
    static float angleNorm(const Ctl& c, float x, float y) {
        float a = std::atan2(y - c.y, x - c.x);
        const float a0 = 0.75f * 3.14159265f;
        float rel = a - a0; while (rel < 0) rel += 2 * 3.14159265f;
        if (rel > 1.5f * 3.14159265f) rel = rel > 1.75f * 3.14159265f ? 0.f : 1.5f * 3.14159265f;
        return rel / (1.5f * 3.14159265f);
    }

    bool onKeyboard(const KeyboardEvent& ev) override {
        if (!ev.press) return false;
        std::string* buf2 = saving ? &saveName : (editing >= 0 ? &editBuf : nullptr);
        if (!buf2) {
            const bool cmd = (ev.mod & (kModifierControl | kModifierSuper)) != 0;
            if (cmd && (ev.key == 'z' || ev.key == 'Z')) { if (ev.mod & kModifierShift) redo(); else undo(); return true; }
            if (cmd && (ev.key == 'y' || ev.key == 'Y')) { redo(); return true; }
            if ((ev.key == kKeyDelete || ev.key == kKeyBackspace) && primary() >= 0 && !helpOpen) { deleteSelected(); return true; }
            if (ev.key == kKeyEscape && helpOpen) { helpOpen = false; repaint(); return true; }
            if (ev.key == kKeyEscape && learnMode) { setLearn(false); return true; }
            return false;
        }
        if (ev.key == kKeyEnter) {
            if (saving) {
                if (saveUserPreset(saveName, val)) { userPresets = listUserPresets(); curUser = sanitizeName(saveName); curFactory = -1; std::copy(val, val + kParamCount, userVals); saving = false; saveError = false; }
                else saveError = true;
            } else endEdit(true);
            repaint(); return true;
        }
        if (ev.key == kKeyEscape) { if (saving) saving = false; else endEdit(false); repaint(); return true; }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!buf2->empty()) buf2->pop_back(); repaint(); return true; }
        if (ev.key >= 32 && ev.key < 127 && buf2->size() < (saving ? 40u : 12u)) { *buf2 += (char)ev.key; repaint(); return true; }
        return true;
    }

    bool onMouse(const MouseEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (ev.press) { statusFlash = 0; tipShown = false; tipSuppressed = tipKey; }
        if (helpOpen) { if (ev.press && ev.button == 1) helpClick(x, y); return true; }
        if (ev.button == 2 && ev.press) {
            if (menuOpen) { const int m = menuHit(x, y); if (m >= 0 && menuItem(m).kind == 1) { pendingDelete = menuItem(m).idx; repaint(); } return true; }
            if (learnMode) { const int i = hit(x, y); if (i >= 0) { const uint32_t p = effParam(controls()[i]); if (p < kParamCount) { char b[24]; snprintf(b, sizeof b, "unmap:%u", p); setState("cmd", b); paramCC[p] = -1; repaint(); } } return true; }
            const int d = dotHit(x, y);
            if (d >= 0) { if (!sel[d]) selectOnly(d); deleteSelected(); return true; }
            return false;
        }
        if (ev.button != 1) return false;
        if (!ev.press) {
            if (active >= 0) { const uint32_t p = effParam(controls()[active]); if (controls()[active].field >= 0) editSelection(false); else if (p < kParamCount) editParameter(p, false); active = -1; endGesture(); return true; }
            if (dragDot >= 0) { editSelection(false); dragDot = -1; endGesture(); return true; }
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
                if (r.kind == 2) { menuOpen = false; saving = true; saveError = false; saveName = curUser; }
                else if (r.kind == 1 && pendingDelete == r.idx) { deleteUserPreset(userPresets[r.idx]); if (curUser == userPresets[r.idx]) curUser.clear(); userPresets = listUserPresets(); pendingDelete = -1; repaint(); return true; }
                else { menuOpen = false; if (r.kind == 0) applyFactory(r.idx); else applyUser(userPresets[r.idx]); }
            } else menuOpen = false;
            pendingDelete = -1; menuHover = -1; repaint(); return true;
        }
        const int hh = headerHit(x, y);
        if (hh == 1) { setLearn(!learnMode); return true; }
        if (hh == 2) { beginGesture(); commit(kMidiOn, val[kMidiOn] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
        if (hh == 3) { undo(); return true; }
        if (hh == 4) { redo(); return true; }
        if (hh == 5) { helpOpen = true; repaint(); return true; }
        if (hh == 6 || hh == 7) { switchAB(hh - 6); return true; }
        if (hh == 8) { copyAB(); return true; }
        const int pb = presetBarHit(x, y);
        if (pb == 0) { step(-1); return true; }
        if (pb == 2) { step(1); return true; }
        if (pb == 1) { userPresets = listUserPresets(); menuOpen = true; menuHover = -1; pendingDelete = -1; repaint(); return true; }
        if (clearHit(x, y)) { setState("cmd", "clear"); for (int& m : paramCC) m = -1; repaint(); return true; }

        int seg = 0; const int i = hit(x, y, &seg);
        if (i >= 0) {
            const Ctl& c = controls()[i]; const uint32_t p = effParam(c);
            if (c.type == DelButton) { deleteSelected(); return true; }
            if (p >= kParamCount) return true;
            if (learnMode) { learnWaiting = (int)p; char b[24]; snprintf(b, sizeof b, "learn:%u", p); setState("cmd", b); repaint(); return true; }
            if (c.type == Toggle) { beginGesture(); applyToSelection(p, val[p] > 0.5f ? 0.f : 1.f, val[p]); endGesture(); return true; }
            if (c.type == Segment) { if (dimmed(c) && c.field == kBSlope) return true; beginGesture(); applyToSelection(p, (float)seg, val[p]); endGesture(); return true; }
            if (ev.mod & (kModifierControl | kModifierSuper)) {
                const float d = c.field == kBFreq ? val[p] : (c.field == kBQ ? (isCut((int)(val[bp(primary(), kBShape)] + .5f)) ? 0.70710678f : 1.f) : defv[p]);
                beginGesture(); applyToSelection(p, d, val[p]); endGesture(); return true;
            }
            if (i == lastClick && ev.time - lastClickTime < 350) {
                lastClick = -1; editing = i;
                char b[40]; fmt(p, b, sizeof b); editBuf = b;
                const size_t sp = editBuf.find(' '); if (sp != std::string::npos) editBuf.resize(sp);
                if (fieldOf(p) == kBFreq && val[p] >= 1000) { char kb[16]; snprintf(kb, sizeof kb, "%.3gk", val[p] / 1000); editBuf = kb; }
                repaint(); return true;
            }
            lastClick = i; lastClickTime = ev.time;
            beginGesture();
            active = i; dragY = y; dragNorm = toNorm(p, val[p]);
            circular = std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y)) > c.r * 0.62f;
            if (c.field >= 0) editSelection(true); else editParameter(p, true);
            if (circular) applyToSelection(p, fromNorm(p, angleNorm(c, x, y)), val[p]);
            repaint(); return true;
        }
        if (!learnMode && inDisplay(x, y)) {
            const int d = dotHit(x, y);
            if (d >= 0) {
                if (ev.mod & kModifierAlt) { beginGesture(); commit(bp(d, kBOn), val[bp(d, kBOn)] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
                if (ev.mod & (kModifierShift | kModifierControl | kModifierSuper)) { sel[d] = !sel[d]; focus = sel[d] ? d : (focus == d ? -1 : focus); repaint(); return true; }
                if (!sel[d]) selectOnly(d); else focus = d;
                beginGesture(); dragDot = d; dragX0 = x; dragY0 = y;
                for (int b = 0; b < kNumBands; ++b) { startF[b] = val[bp(b, kBFreq)]; startG[b] = val[bp(b, kBGain)]; startQ[b] = val[bp(b, kBQ)]; }
                editSelection(true); repaint(); return true;
            }
            const bool dbl = ev.time - lastDispClick < 350 && std::fabs(x - lastDispX) < 6 && std::fabs(y - lastDispY) < 6;
            lastDispClick = ev.time; lastDispX = x; lastDispY = y;
            if (dbl || onCurve(x, y)) {      // add a band and start dragging it
                beginGesture();
                const int b = newBand(hzOfX(x), dbOfY(y));
                if (b < 0) { endGesture(); return true; }
                selectOnly(b); dragDot = b; dragX0 = x; dragY0 = y;
                for (int k = 0; k < kNumBands; ++k) { startF[k] = val[bp(k, kBFreq)]; startG[k] = val[bp(k, kBGain)]; startQ[k] = val[bp(k, kBQ)]; }
                editSelection(true); repaint(); return true;
            }
            boxSel = true; boxX0 = boxX1 = x; boxY0 = boxY1 = y; repaint(); return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (helpOpen) { const int h = helpHit(x, y); if (h != helpHover) { helpHover = h; repaint(); } return true; }
        if (active >= 0) {
            const Ctl& c = controls()[active]; const uint32_t p = effParam(c);
            if (p >= kParamCount) return true;
            if (circular) { applyToSelection(p, fromNorm(p, angleNorm(c, x, y)), val[p]); return true; }
            float sens = (ev.mod & kModifierShift) ? 1500.f : 240.f;
            if (paramDef(p).hints & kHintInteger) sens = 80.f;
            dragNorm = clampf(dragNorm + (dragY - y) / sens, 0.f, 1.f); dragY = y;
            applyToSelection(p, fromNorm(p, dragNorm), val[p]);
            return true;
        }
        if (dragDot >= 0) {
            const float fine = (ev.mod & kModifierShift) ? 0.15f : 1.f;
            const float ratio = std::pow(hzOfX(dragX0 + (x - dragX0) * fine) / hzOfX(dragX0), 1.f), dDb = (dbOfY(dragY0 + (y - dragY0) * fine) - dbOfY(dragY0));
            for (int b = 0; b < kNumBands; ++b) {
                if (!(sel[b] || b == dragDot) || !used(b)) continue;
                setValue(bp(b, kBFreq), clampf(startF[b] * ratio, FMIN, FMAX));
                const int sh = (int)(val[bp(b, kBShape)] + .5f);
                if (isCut(sh)) setValue(bp(b, kBQ), clampf(startQ[b] * std::pow(10.f, dDb / 20.f), 0.1f, 40.f));
                else setValue(bp(b, kBGain), clampf(startG[b] + dDb, -30.f, 30.f));
            }
            return true;
        }
        if (boxSel) { boxX1 = x; boxY1 = y; repaint(); return true; }
        if (menuOpen) { const int m = menuHit(x, y); if (m != menuHover) { menuHover = m; repaint(); } return true; }
        const int hh = headerHit(x, y); if (hh != hoverHeader) { hoverHeader = hh; repaint(); }
        const int pb = presetBarHit(x, y); if (pb != presetHover) { presetHover = pb; repaint(); }
        const bool hc = clearHit(x, y); if (hc != hoverClear) { hoverClear = hc; repaint(); }
        const int h = hit(x, y); if (h != hover) { hover = h; repaint(); }
        const int hd = h < 0 && inDisplay(x, y) ? dotHit(x, y) : -1; if (hd != hoverDot) { hoverDot = hd; repaint(); }
        const int key = h >= 0 ? h : (hh > 0 ? 1000 + hh : (pb >= 0 ? 2000 + pb : (hd >= 0 ? 3000 + hd : (inDisplay(x, y) ? 5000 : -1))));
        tipX = x; tipY = y;
        if (key != tipKey) { const bool was = tipShown; tipKey = key; tipSince = nowSec(); tipShown = false; if (key != tipSuppressed) tipSuppressed = -2; if (was) repaint(); }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override {
        const float s = uiScale(), sx = ev.pos.getX() / s, sy = ev.pos.getY() / s;
        if (menuOpen || saving || helpOpen) return true;
        if (presetBarHit(sx, sy) >= 0) { step(ev.delta.getY() > 0 ? -1 : 1); return true; }
        const float dir = ev.delta.getY() > 0 ? 1.f : -1.f;
        if (inDisplay(sx, sy)) {          // mouse wheel over a dot: Q (of the whole selection if the dot is selected)
            const int d = dotHit(sx, sy); if (d < 0) return false;
            if (!sel[d]) selectOnly(d);
            const float k = std::pow(2.f, dir * ((ev.mod & kModifierShift) ? 0.05f : 0.2f));
            beginGesture(); for (int b = 0; b < kNumBands; ++b) if ((sel[b] || b == d) && used(b)) commit(bp(b, kBQ), clampf(val[bp(b, kBQ)] * k, 0.025f, 40.f)); endGesture();
            return true;
        }
        const int i = hit(sx, sy);
        if (i < 0 || controls()[i].type != Knob) return false;
        const uint32_t p = effParam(controls()[i]); if (p >= kParamCount) return false;
        const float st = (ev.mod & kModifierShift) ? 0.004f : 0.02f;
        beginGesture(); applyToSelection(p, fromNorm(p, toNorm(p, val[p]) + dir * st), val[p]); endGesture();
        return true;
    }

private:
    float val[kParamCount], defv[kParamCount], userVals[kParamCount] = {}, before[kParamCount];
    int paramCC[kParamCount];
    bool sel[kNumBands]; int focus = -1;
    int active = -1, hover = -1, lastClick = -1, editing = -1, hoverDot = -1, dragDot = -1;
    uint lastClickTime = 0, lastDispClick = 0; float lastDispX = 0, lastDispY = 0;
    float dragY = 0, dragNorm = 0, dragX0 = 0, dragY0 = 0; bool circular = false, gesture = false;
    float startF[kNumBands] = {}, startG[kNumBands] = {}, startQ[kNumBands] = {};
    bool boxSel = false; float boxX0 = 0, boxY0 = 0, boxX1 = 0, boxY1 = 0;
    std::vector<Snap> undoStack, redoStack;
    Snap abStore[2]; int abSlot = 0;
    Fft fft; std::vector<cpx> buf; std::vector<float> spec; bool anaValid = false; uint32_t lastWrite = 0; double lastAna = 0; float sampleRateUI = 48000.f;
    std::string editBuf, statusText; int statusFlash = 0;
    int curFactory = 0, presetHover = -1, menuHover = -1, pendingDelete = -1, hoverHeader = 0;
    std::string curUser; std::vector<std::string> userPresets;
    bool menuOpen = false, saving = false, saveError = false; std::string saveName;
    bool learnMode = false, hoverClear = false; int learnWaiting = -1; float lastLearnEcho = 0;
    int tipKey = -1, tipSuppressed = -2; double tipSince = 0; bool tipShown = false; float tipX = 0, tipY = 0;
    bool helpOpen = false; int helpPage = 0, helpHover = -1;
    ResizeHandle resizeHandle;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MicaUI)
};

UI* createUI() { return new MicaUI(); }

END_NAMESPACE_DISTRHO
