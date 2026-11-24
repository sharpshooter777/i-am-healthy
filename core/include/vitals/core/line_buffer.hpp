// SPDX-License-Identifier: Apache-2.0
//
// Splits a byte stream into lines. Network reads arrive in arbitrary pieces;
// this hands back whole lines and keeps the unfinished tail for the next read.
#pragma once

#include <cstddef>
#include <string>

namespace vitals::core {

class LineBuffer {
public:
    void append(const char* data, std::size_t size) { pending_.append(data, size); }

    // Moves the next complete line (without its '\n' or "\r\n") into `out`.
    bool next(std::string& out) {
        const std::size_t nl = pending_.find('\n');
        if (nl == std::string::npos) return false;
        out.assign(pending_, 0, nl);
        if (!out.empty() && out.back() == '\r') out.pop_back();
        pending_.erase(0, nl + 1);
        return true;
    }

    // Bytes held back waiting for a newline. A sender that never ends its
    // lines would grow this without bound, so callers cap it.
    std::size_t pending() const { return pending_.size(); }
    void clear() { pending_.clear(); }

private:
    std::string pending_;
};

}  // namespace vitals::core
