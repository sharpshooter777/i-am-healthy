// SPDX-License-Identifier: Apache-2.0
//
// Power spectrum and band-limited peak search, used to read a rate (pulse,
// breathing) from a periodic signal.
#pragma once

#include <cstddef>
#include <vector>

namespace vitals::core {

struct Spectrum {
    double bin_hz = 0.0;         // frequency step between bins
    std::vector<double> power;   // bins 0 .. nfft/2

    double freq(std::size_t bin) const { return static_cast<double>(bin) * bin_hz; }
};

// One-sided power spectrum of x sampled at fs. The mean is removed and a
// Hann window applied; x is zero-padded to at least min_nfft points to give
// a finer frequency grid.
Spectrum power_spectrum(const std::vector<double>& x, double fs, std::size_t min_nfft = 0);

struct Peak {
    bool valid = false;
    double hz = 0.0;        // peak frequency, refined between bins
    double fraction = 0.0;  // share of the band's power within +-halfwidth of the peak
};

// Strongest component between lo_hz and hi_hz. The fraction is a simple
// signal quality: near 1 for a clean tone, small for noise.
Peak find_peak(const Spectrum& s, double lo_hz, double hi_hz, double halfwidth_hz);

}  // namespace vitals::core
