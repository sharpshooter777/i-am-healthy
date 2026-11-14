// SPDX-License-Identifier: Apache-2.0
//
// Trend fitting and projected time to a limit. The body answers a dose-rate
// change only after a delay, so the monitor warns on the slope of a vital sign
// and on when that slope would reach a limit, not only on limits already crossed.
#pragma once

#include <cstddef>
#include <optional>

namespace vitals::core {

// Least-squares straight line y = slope * t + intercept.
struct LineFit {
    double slope = 0.0;
    double intercept = 0.0;
    bool valid = false;  // false with fewer than two points or no spread in t

    double at(double t) const { return slope * t + intercept; }
};

// Fits a line to n samples (t[i], y[i]). Times are in seconds.
LineFit fit_line(const double* t, const double* y, std::size_t n);

// Which side of the value a limit sits on. Heart rate and breathing rate have
// upper limits; a variability limit may sit on either side.
enum class Bound { Upper, Lower };

// Seconds from t_now until the fitted line reaches the limit.
// Returns 0 when the limit is already reached at t_now, and no value when the
// fit is invalid or the trend is flat or moving away from the limit.
std::optional<double> time_to_limit(const LineFit& fit, double t_now, double limit, Bound bound);

}  // namespace vitals::core
