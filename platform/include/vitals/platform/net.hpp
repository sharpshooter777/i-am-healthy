// SPDX-License-Identifier: Apache-2.0
//
// Minimal TCP for carrying samples between machines: the heart-rate bridge on
// Linux or Windows serves, and the QNX pipeline connects out to it (the QNX
// VM sits behind NAT, so it opens its connections outward).
// QNX and Linux share one BSD-sockets implementation; Windows uses Winsock.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace vitals::platform::net {

using Socket = std::intptr_t;
constexpr Socket invalid_socket = -1;

// Call once before any other function. Idempotent.
bool startup();

// Listens on all interfaces. Port 0 picks a free port; see local_port().
Socket listen_tcp(std::uint16_t port, std::string* error = nullptr);
std::uint16_t local_port(Socket s);

// Waits up to timeout_ms (-1: forever). Returns invalid_socket on timeout or error.
Socket accept_tcp(Socket listener, int timeout_ms, std::string* peer = nullptr);

Socket connect_tcp(const std::string& host, std::uint16_t port, std::string* error = nullptr);

// Sends everything or fails. A closed peer is reported as false, never as a signal.
bool send_all(Socket s, const char* data, std::size_t size);

// Waits up to timeout_ms (-1: forever). Returns the bytes read, 0 when the
// peer closed the connection, -1 on error and -2 on timeout.
long recv_some(Socket s, char* buffer, std::size_t size, int timeout_ms);

void close_socket(Socket s);

}  // namespace vitals::platform::net
