// Behaviour checks: every shape and slope against its analog prototype, a complex curve, linear phase
// (latency, symmetry, accuracy, all qualities), per-band channel placement, mid/side, click-free changes,
// output gain, analyser feed.
#include "../plugins/Mica/ParamDefs.hpp"
#include <cstdio>
#include <vector>
using namespace mica;
static int fails = 0;
#define CHECK(c, msg) do { bool ok_ = (c); printf("%s  %s\n", ok_ ? "PASS" : "FAIL", msg); if (!ok_) fails++; } while (0)
static const int SR = 48000;
struct Buf { std::vector<float> l, r; };
static Buf run(Engine& e, const std::vector<float>& xl, const std::vector<float>& xr) {
    Buf b; b.l.resize(xl.size()); b.r.resize(xl.size());
    for (size_t i = 0; i < xl.size(); i += 256) { const uint32_t n = (uint32_t)std::min<size_t>(256, xl.size() - i); e.process(xl.data() + i, xr.data() + i, b.l.data() + i, b.r.data() + i, n); }
    return b;
}
static std::vector<float> sine(int n, float f, float a) { std::vector<float> v(n); for (int i = 0; i < n; ++i) v[i] = a * std::sin(2 * kPi * f * i / SR); return v; }
static float rms(const std::vector<float>& v, size_t a, size_t z) { double s = 0; for (size_t i = a; i < z; ++i) s += v[i] * v[i]; return (float)std::sqrt(s / (z - a)); }
// measured gain (dB) of the left channel at one frequency
static float gainAt(Engine e, float hz, int ch = 0) {
    const int n = SR; auto x = sine(n, hz, 0.1f); std::vector<float> z(n, 0.f);
    Buf b = ch == 0 ? run(e, x, x) : run(e, x, x);
    const auto& v = ch == 0 ? b.l : b.r;
    return 20 * std::log10(rms(v, n / 2, n) / (0.1f * 0.70710678f));
}
static void addBand(Engine& e, int b, int shape, float hz, float g, float q, int slope = 1, int place = 0) {
    e.p[bp(b, kBUsed)] = 1; e.p[bp(b, kBOn)] = 1; e.p[bp(b, kBShape)] = shape; e.p[bp(b, kBFreq)] = hz; e.p[bp(b, kBGain)] = g;
    e.p[bp(b, kBQ)] = q; e.p[bp(b, kBSlope)] = slope; e.p[bp(b, kBPlace)] = place;
}
static Engine fresh() { Engine e; e.setSampleRate(SR); return e; }
int main() {
    char msg[200];
    { // each shape against its analog prototype at a handful of frequencies
        struct T { int shape, slope; float hz, g, q; const char* nm; } ts[] = {
            { kBell, 1, 1000, 6, 1, "bell +6 dB 1 kHz Q1" }, { kBell, 1, 3000, -12, 4, "bell -12 dB 3 kHz Q4" },
            { kLowShelf, 1, 200, 6, 0.707f, "low shelf +6 dB 200 Hz" }, { kHighShelf, 1, 4000, -6, 0.707f, "high shelf -6 dB 4 kHz" },
            { kLowCut, 0, 200, 0, 0.707f, "low cut 6 dB/oct 200 Hz" }, { kLowCut, 1, 200, 0, 0.707f, "low cut 12 dB/oct" },
            { kLowCut, 2, 200, 0, 0.707f, "low cut 24 dB/oct" }, { kLowCut, 3, 200, 0, 0.707f, "low cut 48 dB/oct" },
            { kHighCut, 2, 2000, 0, 2.f, "high cut 24 dB/oct, resonant Q2" } };
        const float probe[] = { 50, 100, 200, 400, 1000, 2000, 3000, 5000 };
        for (const T& t : ts) {
            Engine e = fresh(); addBand(e, 0, t.shape, t.hz, t.g, t.q, t.slope); e.reset();
            float worst = 0;
            for (float f : probe) {
                const float want = 20 * std::log10(std::abs(e.response(f, 0)));
                if (want < -24) continue;           // deep in a cut's stopband the zero-latency filters cramp near Nyquist
                worst = std::max(worst, std::fabs(gainAt(e, f) - want));
            }
            snprintf(msg, sizeof msg, "zero latency: %-32s matches the analog curve within %.2f dB (to -24 dB)", t.nm, worst);
            CHECK(worst < 0.5f, msg);
        }
    }
    { // a busy six-band curve
        Engine e = fresh(); addBand(e, 0, kLowCut, 40, 0, 0.707f, 2); addBand(e, 1, kLowShelf, 120, 3, 0.707f); addBand(e, 2, kBell, 350, -4, 2);
        addBand(e, 3, kBell, 2500, 5, 1.5f); addBand(e, 4, kHighShelf, 9000, 2.5f, 0.707f); addBand(e, 5, kHighCut, 16000, 0, 0.707f, 3); e.reset();
        float worst = 0; for (float f : { 60.f, 120.f, 350.f, 1000.f, 2500.f, 6000.f, 9000.f }) worst = std::max(worst, std::fabs(gainAt(e, f) - 20 * std::log10(std::abs(e.response(f, 0)))));
        snprintf(msg, sizeof msg, "six-band curve: zero-latency output within %.2f dB of the drawn curve (60 Hz - 9 kHz)", worst); CHECK(worst < 0.6f, msg);
    }
    { // linear phase: latency, symmetric impulse response, accuracy
        const int lat[3] = { 3072, 6144, 12288 };
        for (int q = 0; q < 3; ++q) {
            Engine e = fresh(); e.p[kProcMode] = 1; e.p[kLinQuality] = q; addBand(e, 0, kBell, 1000, 9, 1.5f); e.reset();
            std::vector<float> x(lat[q] * 3, 0.f); x[0] = 1.f; std::vector<float> z(x.size(), 0.f);
            Buf b = run(e, x, x);
            int pk = 0; for (size_t i = 0; i < b.l.size(); ++i) if (std::fabs(b.l[i]) > std::fabs(b.l[pk])) pk = (int)i;
            float asym = 0; for (int k = 1; k < 400; ++k) asym = std::max(asym, std::fabs(b.l[pk + k] - b.l[pk - k]));
            snprintf(msg, sizeof msg, "linear phase %s: reported latency %u, impulse peak at %d, asymmetry %.1e",
                     q == 0 ? "low   " : (q == 1 ? "medium" : "high  "), e.latency(), pk, asym);
            CHECK((int)e.latency() == lat[q] && pk == lat[q] && asym < 1e-4f, msg);
        }
        Engine e = fresh(); e.p[kProcMode] = 1; addBand(e, 0, kLowShelf, 150, 4, 0.707f); addBand(e, 1, kBell, 3000, -6, 3); addBand(e, 2, kHighCut, 12000, 0, 0.707f, 2); e.reset();
        float worst = 0; for (float f : { 300.f, 1000.f, 3000.f, 6000.f, 12000.f }) worst = std::max(worst, std::fabs(gainAt(e, f) - 20 * std::log10(std::abs(e.response(f, 0)))));
        snprintf(msg, sizeof msg, "linear phase (medium): within %.2f dB of the analog curve from 300 Hz to 12 kHz", worst); CHECK(worst < 0.5f, msg);
    }
    { // placement and mid/side
        Engine e = fresh(); addBand(e, 0, kBell, 1000, 12, 1, 1, kPlaceA); e.reset();
        const float gl = gainAt(e, 1000, 0), gr = gainAt(e, 1000, 1);
        snprintf(msg, sizeof msg, "band on left only: left %+.1f dB, right %+.1f dB", gl, gr); CHECK(std::fabs(gl - 12) < 0.3f && std::fabs(gr) < 0.05f, msg);
        Engine m = fresh(); m.p[kChannelMode] = 1; addBand(m, 0, kBell, 1000, 12, 1, 1, kPlaceB); m.reset();
        const float mono = gainAt(m, 1000, 0);
        auto x = sine(SR, 1000, 0.1f); std::vector<float> nx(SR); for (int i = 0; i < SR; ++i) nx[i] = -x[i];
        Buf sb = run(m, x, nx); const float side = 20 * std::log10(rms(sb.l, SR / 2, SR) / (0.1f * 0.70710678f));
        snprintf(msg, sizeof msg, "mid/side, band on side only: mono (mid) signal %+.2f dB, side signal %+.1f dB", mono, side);
        CHECK(std::fabs(mono) < 0.05f && std::fabs(side - 12) < 0.3f, msg);
        Engine z = fresh(); z.p[kProcMode] = 1; addBand(z, 0, kBell, 1000, 12, 1, 1, kPlaceA); z.reset();
        snprintf(msg, sizeof msg, "linear phase keeps placement: left %+.1f dB, right %+.1f dB", gainAt(z, 1000, 0), gainAt(z, 1000, 1));
        CHECK(std::fabs(gainAt(z, 1000, 0) - 12) < 0.3f && std::fabs(gainAt(z, 1000, 1)) < 0.05f, msg);
    }
    { // click-free: enabling a band, changing its shape and sweeping its frequency while a low sine plays
        Engine e = fresh(); addBand(e, 0, kBell, 100, 12, 1); e.p[bp(0, kBOn)] = 0; e.reset();
        auto x = sine(SR, 80, 0.2f); std::vector<float> L(SR), R(SR);
        for (int i = 0; i < SR; i += 128) {
            if (i == 9600) e.p[bp(0, kBOn)] = 1;
            if (i == 24000) e.p[bp(0, kBShape)] = kLowShelf;
            if (i == 36000) e.p[bp(0, kBFreq)] = 1000;
            e.process(x.data() + i, x.data() + i, L.data() + i, R.data() + i, std::min(128, SR - i));
        }
        auto stepMax = [&](int a, int z) { float m = 0; for (int i = a + 1; i < z; ++i) m = std::max(m, std::fabs(L[i] - L[i - 1])); return m; };
        const float worst = stepMax(0, SR), ref = std::max(std::max(stepMax(20000, 24000), stepMax(4000, 9600)), std::max(stepMax(30000, 36000), stepMax(SR - 6000, SR)));
        snprintf(msg, sizeof msg, "enable, shape change and frequency jump are click-free (largest step %.4f, loudest settled %.4f)", worst, ref);
        CHECK(worst < ref * 1.15f, msg);
    }
    { // output gain, analyser feed, flat when empty
        Engine e = fresh(); e.p[kOutGain] = -6; e.reset();
        snprintf(msg, sizeof msg, "no bands, output -6 dB: %.2f dB", gainAt(e, 1000)); CHECK(std::fabs(gainAt(e, 1000) + 6) < 0.05f, msg);
        Engine a = fresh(); run(a, sine(4096, 1000, 0.5f), sine(4096, 1000, 0.5f));
        float mx = 0; for (float v : a.anaPost) mx = std::max(mx, std::fabs(v));
        snprintf(msg, sizeof msg, "analyser ring receives the signal (write index %u, peak %.2f)", a.anaWrite.load(), mx); CHECK(a.anaWrite.load() == 4096 && std::fabs(mx - 0.5f) < 0.01f, msg);
    }
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails ? 1 : 0;
}
