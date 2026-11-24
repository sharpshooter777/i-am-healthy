// SPDX-License-Identifier: Apache-2.0
// All packets here are made-up examples in the standard format, not readings.
#include <doctest.h>

#include <cstdint>
#include <string>

#include "vitals/core/heart_rate.hpp"
#include "vitals/core/line_buffer.hpp"

using namespace vitals::core;

TEST_CASE("heart rate measurement: 8-bit rate with one interval") {
    const std::uint8_t p[] = {0x10, 0x3c, 0x00, 0x04};  // flags: RR present; 60 bpm; 1024/1024 s
    HeartRateSample s;
    REQUIRE(parse_hrm(p, sizeof p, s));
    CHECK(s.bpm == 60);
    CHECK(s.contact == Contact::unknown);
    REQUIRE(s.rr_ms.size() == 1);
    CHECK(s.rr_ms[0] == doctest::Approx(1000.0));
    CHECK(rr_plausible(s));
}

TEST_CASE("heart rate measurement: 16-bit rate, contact bits, energy, two intervals") {
    // flags 0x1f: 16-bit rate, contact supported and detected, energy present, RR present
    const std::uint8_t p[] = {0x1f, 0x4b, 0x00, 0x10, 0x27, 0x33, 0x03, 0x34, 0x03};
    HeartRateSample s;
    REQUIRE(parse_hrm(p, sizeof p, s));
    CHECK(s.bpm == 75);
    CHECK(s.contact == Contact::ok);
    REQUIRE(s.rr_ms.size() == 2);
    CHECK(s.rr_ms[0] == doctest::Approx(819.0 * 1000.0 / 1024.0));
    CHECK(s.rr_ms[1] == doctest::Approx(820.0 * 1000.0 / 1024.0));
    CHECK(rr_plausible(s));
}

TEST_CASE("heart rate measurement: contact lost and no intervals") {
    const std::uint8_t p[] = {0x04, 0x00};  // contact supported, not detected
    HeartRateSample s;
    REQUIRE(parse_hrm(p, sizeof p, s));
    CHECK(s.contact == Contact::lost);
    CHECK(s.rr_ms.empty());
    CHECK_FALSE(rr_plausible(s));
}

TEST_CASE("heart rate measurement: truncated packets are refused") {
    HeartRateSample s;
    const std::uint8_t only_flags[] = {0x10};
    CHECK_FALSE(parse_hrm(only_flags, sizeof only_flags, s));
    const std::uint8_t short_rate[] = {0x01, 0x4b};  // claims 16-bit rate, has one byte
    CHECK_FALSE(parse_hrm(short_rate, sizeof short_rate, s));
    const std::uint8_t odd_rr[] = {0x10, 0x3c, 0x00};  // half an interval
    CHECK_FALSE(parse_hrm(odd_rr, sizeof odd_rr, s));
    CHECK_FALSE(parse_hrm(nullptr, 0, s));
}

TEST_CASE("intervals that disagree with the reported rate are not trusted") {
    HeartRateSample s;
    s.bpm = 60;
    s.rr_ms = {1000.0, 990.0};
    CHECK(rr_plausible(s));
    s.rr_ms = {450.0, 460.0};  // implies about 132 bpm against a reported 60
    CHECK_FALSE(rr_plausible(s));
    s.rr_ms = {1000.0, 150.0};  // an interval no heart produces
    CHECK_FALSE(rr_plausible(s));
    s.rr_ms = {1000.0, 2500.0};
    CHECK_FALSE(rr_plausible(s));
}

TEST_CASE("sample line: round trip") {
    HeartRateSample s;
    s.t_ns = 1700000000123456789LL;
    s.bpm = 72;
    s.contact = Contact::ok;
    s.rr_ms = {833.0, 829.1};
    const std::string line = encode_hr_line(s);
    CHECK(line == "HR1 1700000000123456789 72 1 833.0,829.1");

    HeartRateSample back;
    REQUIRE(decode_hr_line(line + "\r\n", back));
    CHECK(back.t_ns == s.t_ns);
    CHECK(back.bpm == 72);
    CHECK(back.contact == Contact::ok);
    REQUIRE(back.rr_ms.size() == 2);
    CHECK(back.rr_ms[1] == doctest::Approx(829.1));

    s.contact = Contact::unknown;
    s.rr_ms.clear();
    CHECK(encode_hr_line(s) == "HR1 1700000000123456789 72 u -");
    REQUIRE(decode_hr_line(encode_hr_line(s), back));
    CHECK(back.rr_ms.empty());
    CHECK(back.contact == Contact::unknown);
}

TEST_CASE("sample line: malformed lines are refused") {
    HeartRateSample s;
    CHECK_FALSE(decode_hr_line("", s));
    CHECK_FALSE(decode_hr_line("HR2 1 60 1 -", s));
    CHECK_FALSE(decode_hr_line("HR1 1 60 x -", s));
    CHECK_FALSE(decode_hr_line("HR1 1 60 1", s));
    CHECK_FALSE(decode_hr_line("HR1 1 60 1 800.0,,810.0", s));
    CHECK_FALSE(decode_hr_line("HR1 1 60 1 800.0,abc", s));
    CHECK_FALSE(decode_hr_line("HR1 1 60 1 - extra", s));
}

TEST_CASE("line buffer: pieces are joined and the tail is kept") {
    LineBuffer b;
    std::string line;
    b.append("HR1 1 6", 7);
    CHECK_FALSE(b.next(line));
    b.append("0 1 -\r\nHR1 2 61 1 -\nHR", 22);
    REQUIRE(b.next(line));
    CHECK(line == "HR1 1 60 1 -");
    REQUIRE(b.next(line));
    CHECK(line == "HR1 2 61 1 -");
    CHECK_FALSE(b.next(line));
    CHECK(b.pending() == 2);
}

TEST_CASE("strap quality: lost contact or contradicting intervals fail, no intervals pass") {
    HeartRateSample s;
    s.bpm = 60;
    s.contact = Contact::ok;
    CHECK(strap_quality_ok(s));        // no intervals in this packet
    s.rr_ms = {1000.0};
    CHECK(strap_quality_ok(s));
    s.rr_ms = {400.0};                 // 150 bpm intervals against a 60 bpm rate
    CHECK_FALSE(strap_quality_ok(s));
    s.rr_ms = {1000.0};
    s.contact = Contact::lost;
    CHECK_FALSE(strap_quality_ok(s));
    s.contact = Contact::unknown;      // a strap that does not report contact
    CHECK(strap_quality_ok(s));
}
