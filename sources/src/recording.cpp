// SPDX-License-Identifier: Apache-2.0
#include "vitals/sources/recording.hpp"

#include <cstring>
#include <type_traits>

namespace vitals::sources {

namespace {

constexpr char kMagic[8] = {'V', 'M', 'R', 'E', 'C', 0, 0, 0};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint8_t kTypeFrame = 1;
constexpr std::uint8_t kFlagTruth = 1;
// Guards against a corrupt length field asking for gigabytes.
constexpr std::uint32_t kMaxPayload = 64u * 1024u * 1024u;

template <typename T>
void put(std::ostream& out, T v) {
    static_assert(std::is_integral<T>::value, "integers only");
    using U = typename std::make_unsigned<T>::type;
    auto u = static_cast<U>(v);
    char b[sizeof(T)];
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        b[i] = static_cast<char>(u & 0xFFu);
        u = static_cast<U>(u >> 8);
    }
    out.write(b, sizeof(T));
}

void put_f64(std::ostream& out, double d) {
    std::uint64_t u;
    std::memcpy(&u, &d, sizeof u);
    put(out, u);
}

template <typename T>
bool get(std::istream& in, T& v) {
    static_assert(std::is_integral<T>::value, "integers only");
    using U = typename std::make_unsigned<T>::type;
    unsigned char b[sizeof(T)];
    if (!in.read(reinterpret_cast<char*>(b), sizeof(T))) return false;
    U u = 0;
    for (std::size_t i = sizeof(T); i-- > 0;) u = static_cast<U>((u << 8) | b[i]);
    v = static_cast<T>(u);
    return true;
}

bool get_f64(std::istream& in, double& d) {
    std::uint64_t u;
    if (!get(in, u)) return false;
    std::memcpy(&d, &u, sizeof d);
    return true;
}

}  // namespace

RecordingWriter::RecordingWriter(const std::string& path) : out_(path, std::ios::binary | std::ios::trunc) {
    if (!out_) return;
    out_.write(kMagic, sizeof kMagic);
    put(out_, kVersion);
    ok_ = static_cast<bool>(out_);
}

bool RecordingWriter::write(const Frame& f) {
    if (!ok_) return false;
    if (f.data.size() > kMaxPayload) return ok_ = false;
    put(out_, kTypeFrame);
    put(out_, f.camera);
    put(out_, f.seq);
    put(out_, f.timestamp_ns);
    put(out_, f.width);
    put(out_, f.height);
    put(out_, static_cast<std::uint8_t>(f.format));
    put(out_, static_cast<std::uint8_t>(f.truth ? kFlagTruth : 0));
    if (f.truth) {
        put_f64(out_, f.truth->heart_rate_bpm);
        put_f64(out_, f.truth->breathing_rate_bpm);
    }
    put(out_, static_cast<std::uint32_t>(f.data.size()));
    out_.write(reinterpret_cast<const char*>(f.data.data()), static_cast<std::streamsize>(f.data.size()));
    ok_ = static_cast<bool>(out_);
    return ok_;
}

bool RecordingWriter::close() {
    if (out_.is_open()) out_.close();
    return ok_ && !out_.fail();
}

RecordingReader::RecordingReader(const std::string& path) : path_(path), in_(path, std::ios::binary) {
    if (!in_) {
        error_ = "cannot open " + path;
        return;
    }
    char magic[sizeof kMagic];
    std::uint32_t version = 0;
    if (!in_.read(magic, sizeof magic) || std::memcmp(magic, kMagic, sizeof kMagic) != 0) {
        error_ = path + " is not a recording";
    } else if (!get(in_, version) || version != kVersion) {
        error_ = path + ": unsupported recording version " + std::to_string(version);
    }
}

bool RecordingReader::next(Frame& out) {
    if (!error_.empty() || !in_.is_open()) return false;

    std::uint8_t type = 0;
    if (!get(in_, type)) {
        return false;  // clean end of file
    }
    if (type != kTypeFrame) {
        error_ = "unknown record type " + std::to_string(type);
        return false;
    }

    std::uint8_t format = 0, flags = 0;
    std::uint32_t size = 0;
    bool ok = get(in_, out.camera) && get(in_, out.seq) && get(in_, out.timestamp_ns) &&
              get(in_, out.width) && get(in_, out.height) && get(in_, format) && get(in_, flags);
    if (ok && (flags & kFlagTruth)) {
        Truth t;
        ok = get_f64(in_, t.heart_rate_bpm) && get_f64(in_, t.breathing_rate_bpm);
        out.truth = t;
    } else {
        out.truth.reset();
    }
    ok = ok && get(in_, size);
    if (ok && size > kMaxPayload) {
        error_ = "frame payload too large";
        return false;
    }
    if (ok) {
        out.data.resize(size);
        ok = static_cast<bool>(in_.read(reinterpret_cast<char*>(out.data.data()), size));
    }
    if (!ok) {
        error_ = "recording truncated";
        return false;
    }
    if (format != static_cast<std::uint8_t>(PixelFormat::Rgb24)) {
        error_ = "unknown pixel format " + std::to_string(format);
        return false;
    }
    out.format = PixelFormat::Rgb24;
    if (out.data.size() != static_cast<std::size_t>(out.width) * out.height * 3) {
        error_ = "frame size does not match its dimensions";
        return false;
    }
    return true;
}

}  // namespace vitals::sources
