// SPDX-License-Identifier: Apache-2.0
//
// Heart-rate samples from a chest strap: decoding the standard Bluetooth
// Heart Rate Measurement (characteristic 0x2A37), checking whether the
// beat-to-beat intervals can be trusted, and the one-line text format used
// to carry samples between processes and machines.
//
// The strap is ground truth for the camera estimates, so a sample whose
// intervals disagree with its own heart rate is marked, never smoothed over.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vitals::core {

// Electrode contact as the strap reports it.
enum class Contact : std::uint8_t {
    unknown = 0,  // the strap does not report contact
    lost = 1,     // supported, and not detected
    ok = 2,       // supported, and detected
};

struct HeartRateSample {
    std::int64_t t_ns = 0;        // time the sample was received, at its source
    int bpm = 0;
    Contact contact = Contact::unknown;
    std::vector<double> rr_ms;    // beat-to-beat intervals, oldest first
};

// Decodes one Heart Rate Measurement notification. `t_ns` is left untouched.
// Returns false when the packet is too short for the fields its flags claim.
bool parse_hrm(const std::uint8_t* data, std::size_t size, HeartRateSample& out);

// True when every interval is between 300 and 2000 ms and the rate they imply
// is within 25 % of the reported rate. Poor electrode contact produces
// intervals that are noise; this is the check that catches it.
// A sample with no intervals returns false: there is nothing to trust.
bool rr_plausible(const HeartRateSample& s);

// The strap's own quality check, as the combining stage takes it: contact
// not reported lost, and any intervals sent agree with the rate. A sample
// without intervals passes; a strap sends none for some beats.
bool strap_quality_ok(const HeartRateSample& s);

// One sample per line, no trailing newline:
//   HR1 <t_ns> <bpm> <contact u|0|1> <rr_ms,rr_ms,...|->
// Intervals are written with one decimal place.
std::string encode_hr_line(const HeartRateSample& s);

// Parses a line written by encode_hr_line. A trailing '\r' or '\n' is ignored.
bool decode_hr_line(const std::string& line, HeartRateSample& out);

}  // namespace vitals::core
