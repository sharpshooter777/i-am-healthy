// SPDX-License-Identifier: Apache-2.0
// The five combining rules, one test at a time. All values are made up.
#include <doctest.h>

#include "vitals/core/fusion.hpp"

using namespace vitals::core;

namespace {

struct Rig {
    HeartRateFusion f;
    int strap_a = f.add_source("strap A", 1);
    int strap_b = f.add_source("strap B", 2);
    int camera = f.add_source("camera", 3);
};

}  // namespace

TEST_CASE("fusion: nothing reported means no number") {
    Rig r;
    const FusedHeartRate out = r.f.evaluate(10.0);
    CHECK(out.status == FusionStatus::no_source);
    CHECK(out.source == -1);
    CHECK_FALSE(out.alarm_permitted);
    REQUIRE(out.sources.size() == 3);
    CHECK(out.sources[0].state == SourceState::absent);
}

TEST_CASE("fusion: one source is shown but may not raise an alarm (rule 5)") {
    Rig r;
    r.f.update(r.strap_a, 10.0, 72.0, true);
    const FusedHeartRate out = r.f.evaluate(10.5);
    CHECK(out.status == FusionStatus::single);
    CHECK(out.source == r.strap_a);
    CHECK(out.bpm == doctest::Approx(72.0));
    CHECK_FALSE(out.alarm_permitted);
}

TEST_CASE("fusion: two that agree, the higher-ranked is used (rules 1 and 4)") {
    Rig r;
    r.f.update(r.camera, 10.0, 74.0, true);
    r.f.update(r.strap_b, 10.0, 72.0, true);
    const FusedHeartRate out = r.f.evaluate(10.2);
    CHECK(out.status == FusionStatus::corroborated);
    CHECK(out.source == r.strap_b);
    CHECK(out.bpm == doctest::Approx(72.0));
    CHECK(out.alarm_permitted);
    CHECK_FALSE(out.sources[static_cast<std::size_t>(r.camera)].odd_one_out);
}

TEST_CASE("fusion: two that disagree are disputed, with no alarm") {
    Rig r;
    r.f.update(r.strap_a, 10.0, 72.0, true);
    r.f.update(r.camera, 10.0, 95.0, true);
    const FusedHeartRate out = r.f.evaluate(10.2);
    CHECK(out.status == FusionStatus::disputed);
    CHECK(out.source == r.strap_a);  // most trusted, but unconfirmed
    CHECK_FALSE(out.alarm_permitted);
}

TEST_CASE("fusion: one against two, the pair wins even over the top-ranked source (rule 3)") {
    Rig r;
    r.f.update(r.strap_a, 10.0, 110.0, true);  // the odd one, and the most trusted
    r.f.update(r.strap_b, 10.0, 72.0, true);
    r.f.update(r.camera, 10.0, 74.0, true);
    const FusedHeartRate out = r.f.evaluate(10.2);
    CHECK(out.status == FusionStatus::corroborated);
    CHECK(out.source == r.strap_b);
    CHECK(out.bpm == doctest::Approx(72.0));
    CHECK(out.sources[static_cast<std::size_t>(r.strap_a)].odd_one_out);
    CHECK_FALSE(out.sources[static_cast<std::size_t>(r.strap_b)].odd_one_out);
    CHECK_FALSE(out.sources[static_cast<std::size_t>(r.camera)].odd_one_out);
}

TEST_CASE("fusion: a silent source goes stale and the next one takes over") {
    Rig r;
    r.f.update(r.strap_a, 10.0, 72.0, true);
    r.f.update(r.camera, 10.0, 73.0, true);
    for (double t = 11.0; t <= 15.0; t += 1.0) r.f.update(r.camera, t, 73.0, true);  // strap A falls silent
    const FusedHeartRate out = r.f.evaluate(15.1);
    CHECK(out.sources[static_cast<std::size_t>(r.strap_a)].state == SourceState::stale);
    CHECK(out.status == FusionStatus::single);
    CHECK(out.source == r.camera);
    CHECK_FALSE(out.alarm_permitted);
}

TEST_CASE("fusion: quality is judged before a reading is used (rule 2)") {
    Rig r;
    r.f.update(r.strap_a, 10.0, 72.0, false);  // poor electrode contact
    r.f.update(r.strap_b, 10.0, 71.0, true);
    const FusedHeartRate out = r.f.evaluate(10.2);
    CHECK(out.sources[static_cast<std::size_t>(r.strap_a)].state == SourceState::poor_quality);
    CHECK(out.source == r.strap_b);
    CHECK(out.status == FusionStatus::single);
}

TEST_CASE("fusion: values no heart produces are refused (rule 2)") {
    Rig r;
    r.f.update(r.strap_a, 10.0, 12.0, true);
    r.f.update(r.strap_b, 10.0, 260.0, true);
    const FusedHeartRate out = r.f.evaluate(10.2);
    CHECK(out.status == FusionStatus::no_source);
    CHECK(out.sources[0].state == SourceState::out_of_range);
    CHECK(out.sources[1].state == SourceState::out_of_range);
}

TEST_CASE("fusion: a leap is refused, a climb is followed (rule 2)") {
    HeartRateFusion f;
    const int strap = f.add_source("strap", 1);
    f.update(strap, 0.0, 70.0, true);

    // A climb of 2 bpm a second, as at the start of exercise, is believed.
    double bpm = 70.0;
    for (double t = 1.0; t <= 20.0; t += 1.0) {
        bpm += 2.0;
        f.update(strap, t, bpm, true);
        CHECK(f.evaluate(t).sources[0].state == SourceState::ok);
    }
    CHECK(f.evaluate(20.0).bpm == doctest::Approx(110.0));

    // Doubling within a second is not.
    f.update(strap, 21.0, 220.0, true);
    FusedHeartRate out = f.evaluate(21.0);
    CHECK(out.sources[0].state == SourceState::jumped);
    CHECK(out.status == FusionStatus::no_source);

    // Back at a believable value, it is used again.
    f.update(strap, 22.0, 112.0, true);
    CHECK(f.evaluate(22.0).status == FusionStatus::single);
}

TEST_CASE("fusion: a new level that persists is accepted once a heart could have reached it") {
    HeartRateFusion f;  // 5 bpm floor, then 6 bpm a second
    const int strap = f.add_source("strap", 1);
    f.update(strap, 0.0, 70.0, true);
    f.update(strap, 1.0, 130.0, true);   // 60 bpm in one second
    CHECK(f.evaluate(1.0).sources[0].state == SourceState::jumped);
    f.update(strap, 5.0, 130.0, true);   // allowance now 5 + 6*5 = 35 bpm
    CHECK(f.evaluate(5.0).sources[0].state == SourceState::jumped);
    f.update(strap, 10.0, 130.0, true);  // allowance 5 + 6*10 = 65 bpm
    CHECK(f.evaluate(10.0).sources[0].state == SourceState::ok);
    CHECK(f.evaluate(10.0).bpm == doctest::Approx(130.0));
}

TEST_CASE("fusion: agreement tolerance widens with the rate") {
    HeartRateFusion f;
    const int a = f.add_source("a", 1);
    const int b = f.add_source("b", 2);
    f.update(a, 0.0, 60.0, true);
    f.update(b, 0.0, 66.0, true);   // 6 apart at 60: more than 5 bpm and more than 5 %
    CHECK(f.evaluate(0.1).status == FusionStatus::disputed);

    HeartRateFusion g;
    const int c = g.add_source("c", 1);
    const int d = g.add_source("d", 2);
    g.update(c, 0.0, 150.0, true);
    g.update(d, 0.0, 157.0, true);  // 7 apart at 150: within 5 %
    CHECK(g.evaluate(0.1).status == FusionStatus::corroborated);
}

TEST_CASE("fusion: updates for unknown sources are ignored") {
    HeartRateFusion f;
    f.update(0, 0.0, 70.0, true);
    f.update(-1, 0.0, 70.0, true);
    CHECK(f.evaluate(0.0).status == FusionStatus::no_source);
}
