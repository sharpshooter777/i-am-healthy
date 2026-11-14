// SPDX-License-Identifier: Apache-2.0
//
// Camera frames arrive with slightly uneven timestamps. Spectral analysis
// needs evenly spaced samples, so each signal is resampled onto a uniform
// grid using the timestamps taken at the source.
#pragma once

#include <cstddef>
#include <vector>

namespace vitals::core {

// Largest step between consecutive timestamps, in the units of t.
// Returns 0 for fewer than two samples.
double max_gap(const std::vector<double>& t);

// Linearly interpolates (t[i], y[i]) at t0 + k / fs for k = 0 .. n-1.
// t must be strictly increasing and the grid must lie within [t.front(), t.back()].
// Returns false, leaving out empty, if either condition fails.
bool resample_linear(const std::vector<double>& t, const std::vector<double>& y,
                     double t0, double fs, std::size_t n, std::vector<double>& out);

}  // namespace vitals::core
