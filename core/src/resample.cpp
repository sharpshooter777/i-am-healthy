// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/resample.hpp"

namespace vitals::core {

double max_gap(const std::vector<double>& t) {
    double gap = 0.0;
    for (std::size_t i = 1; i < t.size(); ++i) {
        const double d = t[i] - t[i - 1];
        if (d > gap) gap = d;
    }
    return gap;
}

bool resample_linear(const std::vector<double>& t, const std::vector<double>& y,
                     double t0, double fs, std::size_t n, std::vector<double>& out) {
    out.clear();
    if (t.size() < 2 || t.size() != y.size() || fs <= 0.0 || n == 0) return false;
    for (std::size_t i = 1; i < t.size(); ++i) {
        if (!(t[i] > t[i - 1])) return false;
    }
    const double t_end = t0 + static_cast<double>(n - 1) / fs;
    // Allow a hair of rounding at the ends of the grid.
    const double eps = 1e-9;
    if (t0 < t.front() - eps || t_end > t.back() + eps) return false;

    out.resize(n);
    std::size_t j = 0;
    for (std::size_t k = 0; k < n; ++k) {
        const double tk = t0 + static_cast<double>(k) / fs;
        while (j + 2 < t.size() && t[j + 1] < tk) ++j;
        const double span = t[j + 1] - t[j];
        double a = (tk - t[j]) / span;
        if (a < 0.0) a = 0.0;
        if (a > 1.0) a = 1.0;
        out[k] = y[j] + a * (y[j + 1] - y[j]);
    }
    return true;
}

}  // namespace vitals::core
