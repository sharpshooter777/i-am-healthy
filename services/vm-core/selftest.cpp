// SPDX-License-Identifier: Apache-2.0
#include "selftest.hpp"

#include <cmath>
#include <string>

#include "vitals/core/trend.hpp"
#include "vitals/core/version.hpp"
#include "vitals/platform/platform.hpp"

namespace vitals::vmcore {

namespace {

class Checker {
public:
    explicit Checker(std::ostream& out) : out_(out) {}

    void check(const std::string& name, bool ok, const std::string& detail = {}) {
        out_ << (ok ? "  pass  " : "  FAIL  ") << name;
        if (!detail.empty()) out_ << "  (" << detail << ")";
        out_ << '\n';
        if (!ok) ++failures_;
    }

    int failures() const { return failures_; }

private:
    std::ostream& out_;
    int failures_ = 0;
};

bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

}  // namespace

bool run_selftest(std::ostream& out) {
    out << "vm-core " << core::version() << " self-test\n"
        << "  platform  " << platform::name() << "\n"
        << "  os        " << platform::os_description() << "\n";

    Checker c(out);

    // Clock: monotonic, advancing, and with a usable tick.
    const std::int64_t t0 = platform::monotonic_ns();
    std::int64_t t1 = t0;
    for (int i = 0; i < 1000000 && t1 == t0; ++i) {
        t1 = platform::monotonic_ns();
    }
    c.check("monotonic clock reads", t0 >= 0);
    c.check("monotonic clock advances", t1 > t0, std::to_string(t1 - t0) + " ns step");

    const std::int64_t tick = platform::clock_tick_ns();
    c.check("clock tick known", tick > 0, std::to_string(tick) + " ns");

    // Trend: heart rate rising 0.5 bpm per minute from 70 bpm reaches an
    // upper limit of 100 bpm 60 minutes after t = 0.
    const double t[] = {0.0, 60.0, 120.0, 180.0, 240.0};
    double hr[5];
    for (int i = 0; i < 5; ++i) hr[i] = 70.0 + 0.5 * (t[i] / 60.0);
    const core::LineFit fit = core::fit_line(t, hr, 5);
    c.check("trend fit", fit.valid && near(fit.slope, 0.5 / 60.0, 1e-12) && near(fit.intercept, 70.0, 1e-9));

    const auto eta = core::time_to_limit(fit, 240.0, 100.0, core::Bound::Upper);
    c.check("time to limit", eta.has_value() && near(*eta, 3600.0 - 240.0, 1e-6),
            eta ? std::to_string(*eta) + " s" : "none");

    const auto away = core::time_to_limit(fit, 240.0, 50.0, core::Bound::Lower);
    c.check("no projection when moving away", !away.has_value());

    const bool ok = c.failures() == 0;
    out << (ok ? "self-test passed\n" : "self-test FAILED\n");
    return ok;
}

}  // namespace vitals::vmcore
