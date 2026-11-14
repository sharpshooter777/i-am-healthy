// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/spectrum.hpp"

#include <cmath>
#include <complex>

#define POCKETFFT_NO_MULTITHREADING
#include <pocketfft_hdronly.h>

namespace vitals::core {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

Spectrum power_spectrum(const std::vector<double>& x, double fs, std::size_t min_nfft) {
    Spectrum s;
    const std::size_t n = x.size();
    if (n < 2 || fs <= 0.0) return s;

    double mean = 0.0;
    for (double v : x) mean += v;
    mean /= static_cast<double>(n);

    const std::size_t nfft = n > min_nfft ? n : min_nfft;
    std::vector<double> buf(nfft, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(n - 1));
        buf[i] = (x[i] - mean) * w;
    }

    std::vector<std::complex<double>> out(nfft / 2 + 1);
    const pocketfft::shape_t shape{nfft};
    const pocketfft::stride_t stride_in{sizeof(double)};
    const pocketfft::stride_t stride_out{sizeof(std::complex<double>)};
    pocketfft::r2c(shape, stride_in, stride_out, 0, pocketfft::FORWARD, buf.data(), out.data(), 1.0);

    s.bin_hz = fs / static_cast<double>(nfft);
    s.power.resize(out.size());
    for (std::size_t k = 0; k < out.size(); ++k) s.power[k] = std::norm(out[k]);
    return s;
}

Peak find_peak(const Spectrum& s, double lo_hz, double hi_hz, double halfwidth_hz) {
    Peak p;
    if (s.power.size() < 3 || s.bin_hz <= 0.0 || hi_hz <= lo_hz) return p;

    const auto last = s.power.size() - 1;
    auto bin_of = [&](double hz) {
        const double b = std::ceil(hz / s.bin_hz);
        if (b < 1.0) return std::size_t{1};
        const auto ib = static_cast<std::size_t>(b);
        return ib > last - 1 ? last - 1 : ib;
    };
    const std::size_t lo = bin_of(lo_hz);
    std::size_t hi = static_cast<std::size_t>(std::floor(hi_hz / s.bin_hz));
    if (hi > last - 1) hi = last - 1;
    if (hi < lo) return p;

    std::size_t best = lo;
    double band = 0.0;
    for (std::size_t k = lo; k <= hi; ++k) {
        band += s.power[k];
        if (s.power[k] > s.power[best]) best = k;
    }
    if (band <= 0.0) return p;

    // Parabolic interpolation through the peak and its neighbours.
    const double a = s.power[best - 1];
    const double b = s.power[best];
    const double c = s.power[best + 1];
    const double denom = a - 2.0 * b + c;
    double offset = denom != 0.0 ? 0.5 * (a - c) / denom : 0.0;
    if (offset > 0.5) offset = 0.5;
    if (offset < -0.5) offset = -0.5;
    p.hz = (static_cast<double>(best) + offset) * s.bin_hz;

    double near = 0.0;
    for (std::size_t k = lo; k <= hi; ++k) {
        if (std::fabs(s.freq(k) - p.hz) <= halfwidth_hz) near += s.power[k];
    }
    p.fraction = near / band;
    p.valid = true;
    return p;
}

}  // namespace vitals::core
