// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/heart_rate.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace vitals::core {

namespace {

std::uint16_t le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

}  // namespace

bool parse_hrm(const std::uint8_t* data, std::size_t size, HeartRateSample& out) {
    if (data == nullptr || size < 2) return false;
    const std::uint8_t flags = data[0];
    std::size_t i = 1;

    if (flags & 0x01) {  // 16-bit heart rate
        if (size < i + 2) return false;
        out.bpm = le16(data + i);
        i += 2;
    } else {
        out.bpm = data[i];
        i += 1;
    }

    switch ((flags >> 1) & 0x03) {
        case 2: out.contact = Contact::lost; break;
        case 3: out.contact = Contact::ok; break;
        default: out.contact = Contact::unknown; break;
    }

    if (flags & 0x08) {  // energy expended, not used
        if (size < i + 2) return false;
        i += 2;
    }

    out.rr_ms.clear();
    if (flags & 0x10) {  // RR intervals in units of 1/1024 s
        if ((size - i) % 2 != 0) return false;
        for (; i + 1 < size; i += 2) {
            out.rr_ms.push_back(static_cast<double>(le16(data + i)) * 1000.0 / 1024.0);
        }
    }
    return true;
}

bool rr_plausible(const HeartRateSample& s) {
    if (s.rr_ms.empty() || s.bpm <= 0) return false;
    double sum = 0.0;
    for (double rr : s.rr_ms) {
        if (rr < 300.0 || rr > 2000.0) return false;
        sum += rr;
    }
    const double implied = 60000.0 / (sum / static_cast<double>(s.rr_ms.size()));
    return std::fabs(implied - s.bpm) <= 0.25 * s.bpm;
}

bool strap_quality_ok(const HeartRateSample& s) {
    if (s.contact == Contact::lost) return false;
    return s.rr_ms.empty() || rr_plausible(s);
}

std::string encode_hr_line(const HeartRateSample& s) {
    std::ostringstream o;
    o << "HR1 " << s.t_ns << ' ' << s.bpm << ' ';
    switch (s.contact) {
        case Contact::ok: o << '1'; break;
        case Contact::lost: o << '0'; break;
        default: o << 'u'; break;
    }
    o << ' ';
    if (s.rr_ms.empty()) {
        o << '-';
    } else {
        char buf[32];
        for (std::size_t k = 0; k < s.rr_ms.size(); ++k) {
            std::snprintf(buf, sizeof buf, "%.1f", s.rr_ms[k]);
            if (k) o << ',';
            o << buf;
        }
    }
    return o.str();
}

bool decode_hr_line(const std::string& line_in, HeartRateSample& out) {
    std::string line = line_in;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();

    std::istringstream in(line);
    std::string tag, contact, rr;
    long long t = 0;
    int bpm = 0;
    if (!(in >> tag >> t >> bpm >> contact >> rr) || tag != "HR1") return false;
    std::string extra;
    if (in >> extra) return false;

    HeartRateSample s;
    s.t_ns = static_cast<std::int64_t>(t);
    s.bpm = bpm;
    if (contact == "1") s.contact = Contact::ok;
    else if (contact == "0") s.contact = Contact::lost;
    else if (contact == "u") s.contact = Contact::unknown;
    else return false;

    if (rr != "-") {
        std::size_t pos = 0;
        while (pos <= rr.size()) {
            const std::size_t comma = rr.find(',', pos);
            const std::string item = rr.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            char* end = nullptr;
            const double v = std::strtod(item.c_str(), &end);
            if (item.empty() || end == nullptr || *end != '\0') return false;
            s.rr_ms.push_back(v);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    }
    out = std::move(s);
    return true;
}

}  // namespace vitals::core
