// SPDX-License-Identifier: Apache-2.0
// BSD sockets, used on QNX (io-sock) and Linux.
#include "vitals/platform/net.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace vitals::platform::net {

namespace {

int fd(Socket s) { return static_cast<int>(s); }

void set_error(std::string* error, const char* what) {
    if (error) *error = std::string(what) + ": " + std::strerror(errno);
}

// 1 ready, 0 timeout, -1 error.
int wait_readable(int f, int timeout_ms) {
    struct pollfd p {};
    p.fd = f;
    p.events = POLLIN;
    for (;;) {
        const int r = poll(&p, 1, timeout_ms);
        if (r < 0 && errno == EINTR) continue;
        return r < 0 ? -1 : (r == 0 ? 0 : 1);
    }
}

}  // namespace

bool startup() {
    // A peer that disconnects must not kill the process with SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
    return true;
}

Socket listen_tcp(std::uint16_t port, std::string* error) {
    const int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        set_error(error, "socket");
        return invalid_socket;
    }
    const int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a {};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (bind(s, reinterpret_cast<struct sockaddr*>(&a), sizeof a) != 0 || listen(s, 4) != 0) {
        set_error(error, "bind/listen");
        ::close(s);
        return invalid_socket;
    }
    return s;
}

std::uint16_t local_port(Socket s) {
    struct sockaddr_in a {};
    socklen_t len = sizeof a;
    if (getsockname(fd(s), reinterpret_cast<struct sockaddr*>(&a), &len) != 0) return 0;
    return ntohs(a.sin_port);
}

Socket accept_tcp(Socket listener, int timeout_ms, std::string* peer) {
    if (wait_readable(fd(listener), timeout_ms) != 1) return invalid_socket;
    struct sockaddr_in a {};
    socklen_t len = sizeof a;
    const int c = accept(fd(listener), reinterpret_cast<struct sockaddr*>(&a), &len);
    if (c < 0) return invalid_socket;
    const int one = 1;
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    if (peer) {
        char buf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &a.sin_addr, buf, sizeof buf);
        *peer = std::string(buf) + ":" + std::to_string(ntohs(a.sin_port));
    }
    return c;
}

Socket connect_tcp(const std::string& host, std::uint16_t port, std::string* error) {
    struct addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    const std::string service = std::to_string(port);
    const int rc = getaddrinfo(host.c_str(), service.c_str(), &hints, &res);
    if (rc != 0 || res == nullptr) {
        if (error) *error = std::string("resolve ") + host + ": " + gai_strerror(rc);
        return invalid_socket;
    }
    const int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) {
        set_error(error, "socket");
        freeaddrinfo(res);
        return invalid_socket;
    }
    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
        set_error(error, "connect");
        ::close(s);
        freeaddrinfo(res);
        return invalid_socket;
    }
    freeaddrinfo(res);
    const int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    return s;
}

bool send_all(Socket s, const char* data, std::size_t size) {
    while (size > 0) {
        const ssize_t n = ::send(fd(s), data, size, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        data += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

long recv_some(Socket s, char* buffer, std::size_t size, int timeout_ms) {
    const int w = wait_readable(fd(s), timeout_ms);
    if (w == 0) return -2;
    if (w < 0) return -1;
    for (;;) {
        const ssize_t n = ::recv(fd(s), buffer, size, 0);
        if (n < 0 && errno == EINTR) continue;
        return static_cast<long>(n);
    }
}

void close_socket(Socket s) {
    if (s != invalid_socket) ::close(fd(s));
}

}  // namespace vitals::platform::net
