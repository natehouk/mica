// CPU: all 24 bands in use (mixed shapes, 48 dB cuts) in each processing mode, with one band moving; then
// 8 dynamic bands, and 4 spectral-dynamics bands.
#include "../plugins/Mica/ParamDefs.hpp"
#include <chrono>
#include <cstdio>
using namespace mica;
int main() {
    const int SR = 48000; static float in[SR], L[SR], R[SR]; uint32_t r = 1;
    for (float& x : in) { r = r * 1664525u + 1013904223u; x = 0.3f * ((int32_t)r * (1.f / 2147483648.f)); }
    const char* names[5] = { "zero latency", "linear phase (high), redesigning every block", "natural phase", "zero latency + 8 dynamic bands", "zero latency + 4 spectral bands" };
    for (int t = 0; t < 5; ++t) {
        Engine e; e.setSampleRate(SR); e.p[kProcMode] = t < 3 ? t : 0; e.p[kLinQuality] = 2;
        for (int b = 0; b < kNumBands; ++b) { e.p[bp(b, kBUsed)] = 1; e.p[bp(b, kBShape)] = b % 5; e.p[bp(b, kBSlope)] = 3; e.p[bp(b, kBFreq)] = 30.f * std::pow(1.3f, (float)b); e.p[bp(b, kBGain)] = (b % 3) - 1.f; }
        if (t >= 3) for (int b = 0; b < (t == 3 ? 8 : 4); ++b) { const int bb = b * 3; e.p[bp(bb, kBShape)] = kBell; e.p[bp2(bb, kB2Dyn)] = 1; e.p[bp2(bb, kB2Range)] = -6; e.p[bp2(bb, kB2Spectral)] = t == 4; }
        e.reset();
        const int secs = 5; auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < secs; ++k) for (int bl = 0; bl < SR; bl += 256) { e.p[bp(3, kBFreq)] = 500.f + bl % 3000; e.process(in + bl, in + bl, L + bl, R + bl, 256); }
        const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("%s, 24 bands: %.1f%% of one core\n", names[t], 100 * el / secs);
    }
    return 0;
}
