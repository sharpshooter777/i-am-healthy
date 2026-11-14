// SPDX-License-Identifier: Apache-2.0
#include <doctest.h>

#include "vitals/core/trend.hpp"

using vitals::core::Bound;
using vitals::core::fit_line;
using vitals::core::time_to_limit;

TEST_CASE("fit_line recovers an exact line") {
    const double t[] = {0, 10, 20, 30};
    const double y[] = {5, 7, 9, 11};
    const auto fit = fit_line(t, y, 4);
    REQUIRE(fit.valid);
    CHECK(fit.slope == doctest::Approx(0.2));
    CHECK(fit.intercept == doctest::Approx(5.0));
    CHECK(fit.at(40) == doctest::Approx(13.0));
}

TEST_CASE("fit_line stays accurate at large timestamps") {
    // Three hours of session time in seconds from an arbitrary epoch.
    const double base = 1.0e9;
    const double t[] = {base, base + 60, base + 120};
    const double y[] = {80, 81, 82};
    const auto fit = fit_line(t, y, 3);
    REQUIRE(fit.valid);
    CHECK(fit.slope == doctest::Approx(1.0 / 60.0));
    CHECK(fit.at(base + 180) == doctest::Approx(83.0));
}

TEST_CASE("fit_line rejects degenerate input") {
    const double t1[] = {0};
    const double y1[] = {1};
    CHECK_FALSE(fit_line(t1, y1, 1).valid);

    const double t2[] = {5, 5, 5};
    const double y2[] = {1, 2, 3};
    CHECK_FALSE(fit_line(t2, y2, 3).valid);

    CHECK_FALSE(fit_line(nullptr, nullptr, 0).valid);
}

TEST_CASE("time_to_limit for an upper limit") {
    const double t[] = {0, 60};
    const double y[] = {70, 71};  // +1 per minute
    const auto fit = fit_line(t, y, 2);

    SUBCASE("rising toward the limit") {
        const auto eta = time_to_limit(fit, 60, 80, Bound::Upper);
        REQUIRE(eta);
        CHECK(*eta == doctest::Approx(540.0));
    }
    SUBCASE("already at or past the limit") {
        const auto eta = time_to_limit(fit, 60, 71, Bound::Upper);
        REQUIRE(eta);
        CHECK(*eta == 0.0);
    }
}

TEST_CASE("time_to_limit for a lower limit") {
    const double t[] = {0, 60};
    const double y[] = {50, 48};  // falling 2 per minute
    const auto fit = fit_line(t, y, 2);

    const auto eta = time_to_limit(fit, 60, 40, Bound::Lower);
    REQUIRE(eta);
    CHECK(*eta == doctest::Approx(240.0));

    CHECK_FALSE(time_to_limit(fit, 60, 60, Bound::Upper));
}

TEST_CASE("time_to_limit with no trend") {
    const double t[] = {0, 60, 120};
    const double y[] = {70, 70, 70};
    const auto fit = fit_line(t, y, 3);
    CHECK_FALSE(time_to_limit(fit, 120, 100, Bound::Upper));
    CHECK_FALSE(time_to_limit(fit, 120, 50, Bound::Lower));
    CHECK_FALSE(time_to_limit(vitals::core::LineFit{}, 0, 100, Bound::Upper));
}
