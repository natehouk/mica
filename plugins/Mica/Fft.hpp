// Small iterative radix-2 complex FFT (power-of-two sizes). License: MIT.
#pragma once
#include <cmath>
#include <complex>
#include <vector>

namespace mica {

typedef std::complex<float> cpx;

class Fft {
public:
    void init(int n) {
        if (n == size) return;
        size = n; tw.resize(n / 2); rev.resize(n);
        for (int i = 0; i < n / 2; ++i) tw[i] = std::polar(1.0f, (float)(-2.0 * 3.14159265358979 * i / n));
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[i] = r; }
    }
    int n() const { return size; }
    // in-place; inverse = conjugate twiddles, result scaled by 1/n
    void run(cpx* x, bool inverse) const {
        for (int i = 0; i < size; ++i) if (rev[i] > i) std::swap(x[i], x[rev[i]]);
        for (int len = 2; len <= size; len <<= 1) {
            const int half = len >> 1, step = size / len;
            for (int i = 0; i < size; i += len)
                for (int j = 0; j < half; ++j) {
                    const cpx w = inverse ? std::conj(tw[j * step]) : tw[j * step];
                    const cpx u = x[i + j], v = x[i + j + half] * w;
                    x[i + j] = u + v; x[i + j + half] = u - v;
                }
        }
        if (inverse) { const float s = 1.f / size; for (int i = 0; i < size; ++i) x[i] *= s; }
    }
private:
    int size = 0;
    std::vector<cpx> tw;
    std::vector<int> rev;
};

} // namespace mica
