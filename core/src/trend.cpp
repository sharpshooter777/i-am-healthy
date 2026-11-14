// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/trend.hpp"

namespace vitals::core {

LineFit fit_line(const double* t, const double* y, std::size_t n) {
    LineFit fit;
    if (n < 2 || t == nullptr || y == nullptr) {
        return fit;
    }

    // Centre on the means to keep the sums well conditioned for large t.
    double mean_t = 0.0;
    double mean_y = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        mean_t += t[i];
        mean_y += y[i];
    }
    const auto count = static_cast<double>(n);
    mean_t /= count;
    mean_y /= count;

    double sxx = 0.0;
    double sxy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double dt = t[i] - mean_t;
        sxx += dt * dt;
        sxy += dt * (y[i] - mean_y);
    }
    if (sxx <= 0.0) {
        return fit;
    }

    fit.slope = sxy / sxx;
    fit.intercept = mean_y - fit.slope * mean_t;
    fit.valid = true;
    return fit;
}

std::optional<double> time_to_limit(const LineFit& fit, double t_now, double limit, Bound bound) {
    if (!fit.valid) {
        return std::nullopt;
    }
    const double now = fit.at(t_now);
    if (bound == Bound::Upper) {
        if (now >= limit) return 0.0;
        if (fit.slope <= 0.0) return std::nullopt;
    } else {
        if (now <= limit) return 0.0;
        if (fit.slope >= 0.0) return std::nullopt;
    }
    return (limit - now) / fit.slope;
}

}  // namespace vitals::core
