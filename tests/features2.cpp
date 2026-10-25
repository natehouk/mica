// Mica 2.0 behaviour: new shapes and slopes, Natural Phase accuracy and latency, dynamic EQ (internal and
// external), spectral dynamics, character, auto gain, gain scale, phase invert, solo, latency reporting.
#include "../plugins/Mica/ParamDefs.hpp"
#include <cstdio>
#include <vector>
using namespace mica;
static int fails = 0;
#define CHECK(c, ...) do { bool ok_ = (c); char m_[240]; snprintf(m_, sizeof m_, __VA_ARGS__); printf("%s  %s\n", ok_ ? "PASS" : "FAIL", m_); if (!ok_) fails++; } while (0)
static const int SR = 48000;
struct Buf { std::vector<float> l, r; };
static Buf run(Engine& e, const std::vector<float>& xl, const std::vector<float>& xr, const std::vector<float>* sc = nullptr) {
    Buf b; b.l.resize(xl.size()); b.r.resize(xl.size());
    for (size_t i = 0; i < xl.size(); i += 256) { const uint32_t n = (uint32_t)std::min<size_t>(256, xl.size() - i);
        e.process(xl.data() + i, xr.data() + i, sc ? sc->data() + i : nullptr, sc ? sc->data() + i : nullptr, b.l.data() + i, b.r.data() + i, n); }
    return b;
}
static std::vector<float> sine(int n, float f, float a) { std::vector<float> v(n); for (int i = 0; i < n; ++i) v[i] = a * std::sin(2 * kPi * f * i / SR); return v; }
static float rms(const std::vector<float>& v, size_t a, size_t z) { double s = 0; for (size_t i = a; i < z; ++i) s += v[i] * v[i]; return (float)std::sqrt(s / (z - a)); }
static float db(float x) { return 20 * std::log10(x + 1e-12f); }
static float gainAt(Engine e, float hz, float amp = 0.1f) { const int n = SR; auto x = sine(n, hz, amp); Buf b = run(e, x, x); return db(rms(b.l, n / 2, n) / (amp * 0.70710678f)); }
static void addBand(Engine& e, int b, int shape, float hz, float g, float q, int slopeIdx = 1) {
    e.p[bp(b, kBUsed)] = 1; e.p[bp(b, kBOn)] = 1; e.p[bp(b, kBShape)] = shape; e.p[bp(b, kBFreq)] = hz; e.p[bp(b, kBGain)] = g; e.p[bp(b, kBQ)] = q;
    if (isCutShape(shape)) e.p[bp(b, kBSlope)] = slopeIdx; else e.p[bp2(b, kB2Slope)] = slopeIdx;
}
static Engine fresh(int mode = kZeroLatency) { Engine e; e.p[kProcMode] = mode; e.setSampleRate(SR); return e; }
static float curveDb(const Engine& e, float hz) { return db(std::abs(e.response(hz, 0))); }

int main() {
    { // every new shape and a range of slopes, zero latency, against the analog curve (below 6 kHz)
        struct T { int shape, slope; float hz, g, q; const char* nm; } ts[] = {
            { kNotch, 1, 1000, 0, 4, "notch 1 kHz" }, { kBandPass, 1, 800, 0, 1, "band pass 12 dB/oct" }, { kBandPass, 9, 800, 0, 1, "band pass 60 dB/oct" },
            { kTiltShelf, 1, 1000, 6, 0.707f, "tilt shelf +6 dB" }, { kFlatTilt, 1, 1000, -9, 1, "flat tilt -9 dB" }, { kAllPass, 1, 1000, 0, 1, "all pass" },
            { kLowShelf, 15, 300, -8, 0.707f, "low shelf 96 dB/oct" }, { kHighShelf, 0, 2000, 5, 0.707f, "high shelf 6 dB/oct" },
            { kLowCut, 15, 200, 0, 0.707f, "low cut 96 dB/oct" }, { kHighCut, 4, 3000, 0, 0.707f, "high cut 18 dB/oct" } };
        for (const T& t : ts) {
            Engine e = fresh(); addBand(e, 0, t.shape, t.hz, t.g, t.q, t.slope); e.reset();
            float worst = 0;
            for (float f : { 60.f, 150.f, 400.f, 800.f, 1200.f, 2500.f, 5000.f }) { const float want = curveDb(e, f); if (want < -30) continue; worst = std::max(worst, std::fabs(gainAt(e, f) - want)); }
            CHECK(worst < 0.7f, "zero latency: %-22s within %.2f dB of the analog curve", t.nm, worst);
        }
    }
    { // Natural Phase: matches the analog curve right up to 20 kHz, where zero latency cramps; small latency
        Engine z = fresh(kZeroLatency), n = fresh(kNaturalPhase);
        for (Engine* e : { &z, &n }) { addBand(*e, 0, kBell, 16000, 9, 1.5f); addBand(*e, 1, kHighShelf, 12000, -6, 0.707f); e->reset(); }
        float wz = 0, wn = 0;
        for (float f : { 1000.f, 8000.f, 12000.f, 16000.f, 19000.f }) { const float want = curveDb(n, f); wz = std::max(wz, std::fabs(gainAt(z, f) - want)); wn = std::max(wn, std::fabs(gainAt(n, f) - want)); }
        CHECK(wn < 0.3f && wz > 1.f, "natural phase follows the analog curve to 19 kHz within %.2f dB (zero latency is off by %.2f dB)", wn, wz);
        CHECK(n.latency() == 192 && n.latency() * 1000.f / SR < 5.f, "natural phase latency %u samples (%.1f ms)", n.latency(), n.latency() * 1000.f / SR);
        // impulse: the energy is after the reported latency (minimum-phase-like), not spread symmetrically
        Engine m = fresh(kNaturalPhase); addBand(m, 0, kBell, 200, 12, 2); m.reset();
        std::vector<float> imp(8192, 0.f); imp[0] = 1; Buf b = run(m, imp, imp);
        double pre = 0, post = 0; for (int i = 0; i < 8192; ++i) (i < (int)m.latency() - 4 ? pre : post) += b.l[i] * b.l[i];
        CHECK(pre < 1e-3 * post, "natural phase: %.4f%% of the impulse energy arrives early", 100 * pre / post);
    }
    { // gain scale and phase invert
        Engine e = fresh(); addBand(e, 0, kBell, 1000, 6, 1); e.p[kGainScale] = 50; e.reset();
        CHECK(std::fabs(gainAt(e, 1000) - 3) < 0.1f, "gain scale 50%%: +6 dB bell measures %+.2f dB", gainAt(e, 1000));
        e.p[kGainScale] = -100; e.reset(); CHECK(std::fabs(gainAt(e, 1000) + 6) < 0.1f, "gain scale -100%%: %+.2f dB", gainAt(e, 1000));
        Engine v = fresh(); v.p[kPhaseInvert] = 1; v.reset(); auto x = sine(4800, 500, 0.3f); Buf b = run(v, x, x);
        float d = 0; for (int i = 100; i < 4800; ++i) d = std::max(d, std::fabs(b.l[i] + x[i]));
        CHECK(d < 1e-4f, "phase invert: output = -input (error %.6f)", d);
    }
    { // auto gain keeps pink-noise level roughly constant
        Engine e = fresh(); addBand(e, 0, kHighShelf, 3000, 9, 0.707f); addBand(e, 1, kBell, 300, 6, 0.7f); e.p[kAutoGain] = 1; e.reset();
        CHECK(e.autoGain() < -2.f, "auto gain compensates a boost: %+.2f dB", e.autoGain());
        Engine f = fresh(); f.p[kAutoGain] = 1; f.reset(); CHECK(std::fabs(f.autoGain()) < 0.01f, "auto gain is 0 dB on a flat curve (%.3f)", f.autoGain());
    }
    { // dynamic EQ: a bell that cuts only when its band is loud
        auto mk = [](float src) { Engine e = fresh(); addBand(e, 0, kBell, 1000, 0, 2); e.p[bp2(0, kB2Dyn)] = 1; e.p[bp2(0, kB2Range)] = -9; e.p[bp2(0, kB2Thresh)] = -30; e.p[bp2(0, kB2Source)] = src; e.reset(); return e; };
        Engine quiet = mk(0), loud = mk(0);
        const float gq = gainAt(quiet, 1000, 0.005f), gl = gainAt(loud, 1000, 0.5f);
        CHECK(std::fabs(gq) < 0.3f && gl < -8.5f && gl > -9.5f, "dynamic bell: quiet tone %+.2f dB, loud tone %+.2f dB (range -9)", gq, gl);
        Engine off = mk(0); const float other = gainAt(off, 100, 0.5f);
        CHECK(std::fabs(other) < 0.3f, "dynamic bell leaves a loud tone outside its band alone (%+.2f dB)", other);
        // external sidechain: the band reacts to the key, not the input
        Engine ex = mk(1); const int n = SR; auto x = sine(n, 1000, 0.05f), key = sine(n, 1000, 0.5f); std::vector<float> none(n, 0.f);
        Buf k1 = run(ex, x, x, &key); Engine ex2 = mk(1); Buf k0 = run(ex2, x, x, &none);
        const float a = db(rms(k1.l, n / 2, n) / rms(x, n / 2, n)), c = db(rms(k0.l, n / 2, n) / rms(x, n / 2, n));
        CHECK(a < -8.f && std::fabs(c) < 0.3f, "external sidechain: key on %+.2f dB, key silent %+.2f dB", a, c);
        // display feed
        { const int n2 = SR; auto x2 = sine(n2, 1000, 0.5f); run(loud, x2, x2); }
        CHECK(loud.dynShow[0].load() < -8.f, "dynamic gain change is reported for the display (%.2f dB)", loud.dynShow[0].load());
        // expansion upward: positive range on a quiet band does nothing, on a loud one boosts
        Engine up = fresh(); addBand(up, 0, kHighShelf, 4000, 0, 0.707f); up.p[bp2(0, kB2Dyn)] = 1; up.p[bp2(0, kB2Range)] = 6; up.p[bp2(0, kB2Thresh)] = -40; up.reset();
        CHECK(gainAt(up, 8000, 0.3f) > 5.5f, "dynamic high shelf boosts by up to +6 dB when loud (%+.2f dB)", gainAt(up, 8000, 0.3f));
        // works in all three processing modes
        for (int md : { kLinearPhase, kNaturalPhase }) { Engine e = fresh(md); addBand(e, 0, kBell, 1000, 3, 2); e.p[bp2(0, kB2Dyn)] = 1; e.p[bp2(0, kB2Range)] = -9; e.p[bp2(0, kB2Thresh)] = -30; e.reset();
            CHECK(gainAt(e, 1000, 0.5f) < -5.5f && std::fabs(gainAt(e, 1000, 0.003f) - 3) < 0.4f, "%s: dynamic bell %+.2f dB loud, %+.2f dB quiet (static +3)", md == kLinearPhase ? "linear phase" : "natural phase", gainAt(e, 1000, 0.5f), gainAt(e, 1000, 0.003f)); }
    }
    { // spectral dynamics: inside one wide band, a loud tone is pulled down, a quiet one beside it is not
        Engine e = fresh(); addBand(e, 0, kBell, 1500, 0, 0.5f); e.p[bp2(0, kB2Dyn)] = 1; e.p[bp2(0, kB2Range)] = -12; e.p[bp2(0, kB2Thresh)] = -30; e.p[bp2(0, kB2Spectral)] = 1; e.reset();
        CHECK(e.wantedLatency() == 1024, "spectral dynamics adds %u samples of latency", e.wantedLatency());
        const int n = SR * 2; std::vector<float> x(n); for (int i = 0; i < n; ++i) x[i] = 0.4f * std::sin(2 * kPi * 1200 * i / SR) + 0.005f * std::sin(2 * kPi * 2000 * i / SR);
        Buf b = run(e, x, x);
        auto tone = [&](const std::vector<float>& v, float f, int from) { double re = 0, im = 0; for (int i = from; i < n; ++i) { re += v[i] * std::cos(2 * kPi * f * i / SR); im += v[i] * std::sin(2 * kPi * f * i / SR); } return std::sqrt(re * re + im * im) * 2 / (n - from); };
        const int lat = (int)e.latency(); std::vector<float> al(n, 0.f); for (int i = lat; i < n; ++i) al[i - lat] = b.l[i];
        const float loudDb = db(tone(al, 1200, n / 2) / 0.4f), quietDb = db(tone(al, 2000, n / 2) / 0.005f);
        CHECK(loudDb < -8.f && std::fabs(quietDb) < 1.5f, "spectral: loud 1.2 kHz tone %+.2f dB, quiet 2 kHz tone %+.2f dB", loudDb, quietDb);
        Engine s = e; Engine f2 = fresh(); addBand(f2, 0, kBell, 1500, 0, 0.5f); f2.reset();
        Buf c = run(f2, x, x); float d = 0; for (int i = 0; i < n; ++i) d = std::max(d, std::fabs(c.l[i] - x[i]));
        CHECK(d < 1e-4f, "with spectral off, a flat band is transparent (%.6f)", d);
    }
    { // character: clean is transparent, gentle/warm add harmonics only where the EQ acts
        auto thd = [](Engine e) { const int n = SR; auto x = sine(n, 1000, 0.5f); Buf b = run(e, x, x);
            const int M = n / 2; double re = 0, im = 0, tot = 0;
            for (int i = n / 2; i < n; ++i) { const double ph = 2.0 * 3.14159265358979 * 1000.0 * i / SR; re += b.l[i] * std::cos(ph); im += b.l[i] * std::sin(ph); tot += (double)b.l[i] * b.l[i]; }
            const double amp = 2.0 * std::sqrt(re * re + im * im) / M, fund = amp * amp / 2, all = tot / M;
            return (float)(10 * std::log10(std::max(1e-20, (all - fund)) / fund)); };
        for (int chr : { kGentle, kWarm }) {
            Engine flat = fresh(); flat.p[kCharacter] = chr; addBand(flat, 0, kBell, 1000, 0, 1); flat.reset();
            Engine boost = fresh(); boost.p[kCharacter] = chr; addBand(boost, 0, kBell, 1000, 9, 1); boost.reset();
            Engine clean = fresh(); addBand(clean, 0, kBell, 1000, 9, 1); clean.reset();
            CHECK(thd(flat) < -70 && thd(boost) > -60 && thd(clean) < -70, "%s: flat EQ distortion %.0f dB, +9 dB bell %.0f dB (clean %.0f dB)", chr == kGentle ? "gentle" : "warm", thd(flat), thd(boost), thd(clean));
        }
    }
    { // solo: hear the band's region only
        Engine e = fresh(); addBand(e, 0, kBell, 1000, 6, 2); e.soloBand = 0; e.reset();
        CHECK(std::fabs(gainAt(e, 1000)) < 0.3f && gainAt(e, 100) < -15 && gainAt(e, 10000) < -15, "solo bell: %+.1f dB at the band, %+.1f / %+.1f dB outside", gainAt(e, 1000), gainAt(e, 100), gainAt(e, 10000));
        Engine c = fresh(); addBand(c, 0, kLowCut, 300, 0, 0.707f, 2); c.soloBand = 0; c.reset();
        CHECK(gainAt(c, 60) > -1.f && gainAt(c, 3000) < -30, "solo low cut plays what it removes: %+.1f dB at 60 Hz, %+.1f dB at 3 kHz", gainAt(c, 60), gainAt(c, 3000));
    }
    { // latency reporting
        Engine e = fresh(kLinearPhase); CHECK(e.latency() == 6144 && e.wantedLatency() == 6144, "linear phase (medium) latency %u", e.latency());
        Engine z = fresh(); CHECK(z.latency() == 0, "zero latency: 0");
    }
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails;
}
