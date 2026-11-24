// SPDX-License-Identifier: Apache-2.0
#include <doctest.h>

#include <string>

#include "vitals/platform/net.hpp"

namespace net = vitals::platform::net;

TEST_CASE("tcp: loopback round trip, timeout and close") {
    REQUIRE(net::startup());
    std::string error;
    const net::Socket listener = net::listen_tcp(0, &error);
    REQUIRE_MESSAGE(listener != net::invalid_socket, error);
    const std::uint16_t port = net::local_port(listener);
    REQUIRE(port != 0);

    // Nobody has connected yet.
    CHECK(net::accept_tcp(listener, 0) == net::invalid_socket);

    const net::Socket client = net::connect_tcp("127.0.0.1", port, &error);
    REQUIRE_MESSAGE(client != net::invalid_socket, error);
    std::string peer;
    const net::Socket served = net::accept_tcp(listener, 2000, &peer);
    REQUIRE(served != net::invalid_socket);
    CHECK(peer.rfind("127.0.0.1:", 0) == 0);

    char buf[64];
    CHECK(net::recv_some(client, buf, sizeof buf, 50) == -2);  // timeout, nothing sent yet

    const std::string msg = "HR1 1 60 1 -\n";
    REQUIRE(net::send_all(served, msg.data(), msg.size()));
    std::string got;
    while (got.size() < msg.size()) {
        const long n = net::recv_some(client, buf, sizeof buf, 2000);
        REQUIRE(n > 0);
        got.append(buf, static_cast<std::size_t>(n));
    }
    CHECK(got == msg);

    net::close_socket(served);
    CHECK(net::recv_some(client, buf, sizeof buf, 2000) == 0);  // peer closed
    net::close_socket(client);
    net::close_socket(listener);
}

TEST_CASE("tcp: connecting to a closed port fails with a message") {
    REQUIRE(net::startup());
    const net::Socket listener = net::listen_tcp(0);
    REQUIRE(listener != net::invalid_socket);
    const std::uint16_t port = net::local_port(listener);
    net::close_socket(listener);
    std::string error;
    CHECK(net::connect_tcp("127.0.0.1", port, &error) == net::invalid_socket);
    CHECK_FALSE(error.empty());
}
