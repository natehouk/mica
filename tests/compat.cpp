// 1.x compatibility: random 1.x settings (2.0 parameters at their defaults) through the frozen 1.0.1 engine
// and the 2.0 engine, zero latency and linear phase, 44.1-96 kHz: the outputs must match.
#include "../plugins/Mica/ParamDefs.hpp"
#include "legacy/MicaEngine1.hpp"
#include <cstdio>
#include <random>
using namespace mica;
int main() {
    std::mt19937 g(11); std::uniform_real_distribution<float> u(0, 1);
    int bad = 0; float worstAll = -1000;
    for (int sr : { 44100, 48000, 96000 }) for (int mode = 0; mode < 2; ++mode) for (int trial = 0; trial < 12; ++trial) {
        Engine e; mica_old::Engine o;
        e.p[kProcMode] = o.p[mica_old::kProcMode] = (float)mode;
        e.p[kLinQuality] = o.p[mica_old::kLinQuality] = (float)(trial % 3);
        e.p[kChannelMode] = o.p[mica_old::kChannelMode] = (float)(trial % 2);
        const int nb = 1 + trial % 8;
        for (int b = 0; b < nb; ++b) {
            const int bb = (b * 5 + trial) % kNumBands;
            const float v[8] = { 1.f, u(g) < 0.9f ? 1.f : 0.f, std::floor(u(g) * 5), 20.f * std::pow(1000.f, u(g)), -18 + 36 * u(g), 0.2f * std::pow(50.f, u(g)), std::floor(u(g) * 4), std::floor(u(g) * 3) };
            for (int f = 0; f < kBandFields; ++f) { e.p[bp(bb, f)] = v[f]; o.p[mica_old::bp(bb, f)] = v[f]; }
        }
        e.setSampleRate((float)sr); o.setSampleRate((float)sr);
        const int N = sr; std::vector<float> x(N), L1(N), R1(N), L2(N), R2(N);
        for (int i = 0; i < N; ++i) x[i] = 0.3f * (2 * u(g) - 1);
        for (int i = 0; i < N; i += 512) {
            const int n = std::min(512, N - i);
            if (i == N / 2) { // a parameter move halfway, to exercise smoothing and kernel cross-fades
                const int bb = trial % kNumBands; e.p[bp(bb, kBFreq)] *= 1.5f; o.p[mica_old::bp(bb, kBFreq)] *= 1.5f; }
            o.process(x.data() + i, x.data() + i, L1.data() + i, R1.data() + i, n);
            e.process(x.data() + i, x.data() + i, L2.data() + i, R2.data() + i, n);
        }
        float worst = 0, peak = 0.3f;   // relative to the input level (a channel may be cut nearly to silence)
        for (int i = 0; i < N; ++i) { worst = std::max(worst, std::max(std::fabs(L1[i] - L2[i]), std::fabs(R1[i] - R2[i]))); }
        const float rel = 20 * std::log10(worst / peak + 1e-12f);
        worstAll = std::max(worstAll, rel);
        if (rel > -80) { ++bad; printf("FAIL sr %d mode %d trial %d: difference %.1f dB below the input\n", sr, mode, trial, -rel); }
    }
    printf("1.x compatibility: largest difference %.1f dB below the input level, %d failures\n", -worstAll, bad);
    return bad;
}
