// SPDX-License-Identifier: Apache-2.0
#include <doctest.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "vitals/core/image.hpp"
#include "vitals/core/pulse.hpp"
#include "vitals/sources/recording.hpp"
#include "vitals/sources/synthetic.hpp"

using namespace vitals::sources;

namespace {

std::string temp_path(const char* name) {
    // The working directory is the build folder under ctest and the deploy
    // folder on a QNX target; both are writable.
    return std::string("test-") + name + ".vmrec";
}

// Runs a source through region mean and pulse tracking, comparing each
// estimate with the true rate averaged over the same window.
double worst_error(FrameSource& src, double window_s, int* estimates) {
    vitals::core::PulseTracker tracker(window_s);
    Frame f;
    double worst = 0.0, next = -1.0, truth_sum = 0.0;
    int truth_n = 0;
    *estimates = 0;
    while (src.next(f)) {
        const auto m = vitals::core::mean_rgb(f.data.data(), f.width, f.height, f.width * 3, {0, 0, f.width, f.height});
        REQUIRE(m.valid);
        tracker.push({f.timestamp_s(), m.r, m.g, m.b});
        REQUIRE(f.truth);
        truth_sum += f.truth->heart_rate_bpm;
        ++truth_n;
        if (next < 0.0) next = f.timestamp_s() + window_s;
        if (f.timestamp_s() < next) continue;
        next += window_s;  // non-overlapping windows, so truth averages line up

        const auto hr = tracker.estimate();
        REQUIRE(hr.ok());
        worst = std::fmax(worst, std::fabs(hr.bpm - truth_sum / truth_n));
        ++*estimates;
        truth_sum = 0.0;
        truth_n = 0;
    }
    return worst;
}

}  // namespace

TEST_CASE("synthetic source: sequence, timestamps and truth") {
    SyntheticConfig cfg;
    cfg.seconds = 2.0;
    cfg.hr_start_bpm = 70.0;
    cfg.hr_slope_bpm_per_min = 6.0;
    SyntheticSource src(cfg);
    Frame f;
    std::uint64_t n = 0;
    std::int64_t last_ts = -1;
    while (src.next(f)) {
        CHECK(f.seq == n);
        CHECK(f.timestamp_ns > last_ts);
        CHECK(f.data.size() == 64u * 64u * 3u);
        REQUIRE(f.truth);
        last_ts = f.timestamp_ns;
        ++n;
    }
    CHECK(n == 60);
    CHECK(f.truth->heart_rate_bpm == doctest::Approx(70.0 + 6.0 * f.timestamp_s() / 60.0));
}

TEST_CASE("synthetic source: dropped frames leave gaps in time, not in seq") {
    SyntheticConfig cfg;
    cfg.seconds = 10.0;
    cfg.drop_probability = 0.2;
    cfg.timing_jitter_ms = 0.0;
    SyntheticSource src(cfg);
    Frame f;
    std::uint64_t n = 0;
    while (src.next(f)) ++n;
    CHECK(n < 280);
    CHECK(n > 200);
    CHECK(f.seq == n - 1);
}

TEST_CASE("recording round trip") {
    const std::string path = temp_path("roundtrip");
    SyntheticConfig cfg;
    cfg.seconds = 1.0;
    cfg.camera = 3;
    {
        SyntheticSource src(cfg);
        RecordingWriter out(path);
        REQUIRE(out.ok());
        Frame f;
        while (src.next(f)) REQUIRE(out.write(f));
        REQUIRE(out.close());
    }

    SyntheticSource again(cfg);
    RecordingReader in(path);
    REQUIRE(in.error().empty());
    Frame a, b;
    int n = 0;
    while (again.next(a)) {
        REQUIRE(in.next(b));
        CHECK(b.camera == 3);
        CHECK(b.seq == a.seq);
        CHECK(b.timestamp_ns == a.timestamp_ns);
        CHECK(b.data == a.data);
        REQUIRE(b.truth);
        CHECK(b.truth->heart_rate_bpm == a.truth->heart_rate_bpm);
        ++n;
    }
    CHECK(n == 30);
    CHECK_FALSE(in.next(b));
    CHECK(in.error().empty());
    std::remove(path.c_str());
}

TEST_CASE("recording reader rejects bad files") {
    CHECK_FALSE(RecordingReader("does-not-exist.vmrec").error().empty());

    const std::string path = temp_path("bad");
    { std::ofstream(path, std::ios::binary) << "not a recording at all"; }
    RecordingReader r(path);
    CHECK_FALSE(r.error().empty());
    Frame f;
    CHECK_FALSE(r.next(f));
    std::remove(path.c_str());
}

TEST_CASE("end to end: synthetic camera to heart rate within 3 bpm") {
    SyntheticConfig cfg;
    cfg.seconds = 60.0;
    cfg.hr_start_bpm = 68.0;

    SUBCASE("steady") {}
    SUBCASE("rising 4 bpm per minute") { cfg.hr_slope_bpm_per_min = 4.0; }
    SUBCASE("15 fps with dropped frames") {
        cfg.fps = 15.0;
        cfg.drop_probability = 0.02;
    }
    SUBCASE("noisier sensor") { cfg.noise_level = 6.0; }

    SyntheticSource src(cfg);
    int estimates = 0;
    const double worst = worst_error(src, 10.0, &estimates);
    CHECK(estimates >= 4);
    CHECK(worst < 3.0);
}
