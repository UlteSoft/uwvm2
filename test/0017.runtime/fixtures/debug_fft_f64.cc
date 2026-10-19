// Same radix-2 kernel as u2bench/wasm/src/cc/science_fft_f64.cc.
// One forward/inverse pair by default; override repetitions for long PMU runs.
// 2048 values, deterministic seed and checksum.
#ifndef UWVM_DEBUG_FFT_REPS
#define UWVM_DEBUG_FFT_REPS 1
#endif
#include <stdio.h>
#include <time.h>
#include <stdint.h>
static uint64_t u2bench_now_ns()
{
    timespec t{};
    if(clock_gettime(CLOCK_MONOTONIC, &t)) { return 0; }
    return uint64_t(t.tv_sec) * 1000000000ull + uint64_t(t.tv_nsec);
}
static void u2bench_print_time_ns(uint64_t ns) { printf("Time: %.6f ms\n", double(ns) / 1000000.0); }
static void u2bench_sink_f64(double v) { static volatile double sink; sink = v; }
static uint32_t u2bench_xorshift32(uint32_t* state)
{
    uint32_t x = *state; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *state = x;
}

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

static inline uint32_t bit_reverse(uint32_t x, uint32_t bits) {
    uint32_t r = 0;
    for (uint32_t i = 0; i < bits; ++i) {
        r = (r << 1) | (x & 1u);
        x >>= 1u;
    }
    return r;
}

static void fft_inplace(
    double* re,
    double* im,
    const double* wr,
    const double* wi,
    const uint16_t* rev,
    uint32_t n,
    bool inverse
) {
    // Bit-reversal permutation.
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = rev[i];
        if (i < j) {
            const double tr = re[i];
            const double ti = im[i];
            re[i] = re[j];
            im[i] = im[j];
            re[j] = tr;
            im[j] = ti;
        }
    }

    for (uint32_t len = 2; len <= n; len <<= 1) {
        const uint32_t half = len >> 1;
        const uint32_t step = n / len;
        for (uint32_t i = 0; i < n; i += len) {
            for (uint32_t j = 0; j < half; ++j) {
                const uint32_t k = j * step;
                const double wre = wr[k];
                const double wim = inverse ? -wi[k] : wi[k];

                const uint32_t i0 = i + j;
                const uint32_t i1 = i0 + half;

                const double ure = re[i0];
                const double uim = im[i0];
                const double vre = re[i1] * wre - im[i1] * wim;
                const double vim = re[i1] * wim + im[i1] * wre;

                re[i0] = ure + vre;
                im[i0] = uim + vim;
                re[i1] = ure - vre;
                im[i1] = uim - vim;
            }
        }
    }

    if (inverse) {
        const double inv_n = 1.0 / (double)n;
        for (uint32_t i = 0; i < n; ++i) {
            re[i] *= inv_n;
            im[i] *= inv_n;
        }
    }
}

int main() {
    constexpr uint32_t kN = 2048;
    constexpr uint32_t kBits = 11;
    constexpr int kReps = UWVM_DEBUG_FFT_REPS;
    static_assert(kReps >= 1 && kReps <= 120);
    constexpr double kPi = 3.14159265358979323846264338327950288;

    double* re = (double*)malloc((size_t)kN * sizeof(double));
    double* im = (double*)malloc((size_t)kN * sizeof(double));
    double* wr = (double*)malloc((size_t)(kN / 2) * sizeof(double));
    double* wi = (double*)malloc((size_t)(kN / 2) * sizeof(double));
    uint16_t* rev = (uint16_t*)malloc((size_t)kN * sizeof(uint16_t));
    if (!re || !im || !wr || !wi || !rev) {
        printf("malloc failed\n");
        return 1;
    }

    for (uint32_t k = 0; k < kN / 2; ++k) {
        const double ang = -2.0 * kPi * (double)k / (double)kN;
        wr[k] = cos(ang);
        wi[k] = sin(ang);
    }
    for (uint32_t i = 0; i < kN; ++i) {
        rev[i] = (uint16_t)bit_reverse(i, kBits);
    }

    uint32_t state = 1;
    for (uint32_t i = 0; i < kN; ++i) {
        const uint32_t r = u2bench_xorshift32(&state);
        re[i] = (double)(r & 0xffffu) * 0.00001;
        im[i] = (double)((r >> 16) & 0xffffu) * 0.00001;
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int rep = 0; rep < kReps; ++rep) {
        fft_inplace(re, im, wr, wi, rev, kN, false);
        fft_inplace(re, im, wr, wi, rev, kN, true);
        re[rep & (kN - 1)] += 0.0000001;
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_f64(re[0] + im[1]);
    printf("Checksum: %.17g\n", re[0] + im[1]);
    u2bench_print_time_ns(t1 - t0);

    free(rev);
    free(wi);
    free(wr);
    free(im);
    free(re);
    return 0;
}
