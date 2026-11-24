// SPDX-License-Identifier: Apache-2.0
// Winsock implementation, for running the heart-rate bridge on Windows.
#include "vitals/platform/net.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

namespace vitals::platform::net {

namespace {

SOCKET sock(Socket s) { return static_cast<SOCKET>(s); }

void set_error(std::string* error, const char* what) {
    if (error) *error = std::string(what) + ": Winsock error " + std::to_string(WSAGetLastError());
}

int wait_readable(SOCKET s, int timeout_ms) {
    WSAPOLLFD p {};
    p.fd = s;
    p.events = POLLRDNORM;
    const int r = WSAPoll(&p, 1, timeout_ms);
    return r < 0 ? -1 : (r == 0 ? 0 : 1);
}

}  // namespace

bool startup() {
    static bool done = false;
    if (done) return true;
    WSADATA d;
    done = WSAStartup(MAKEWORD(2, 2), &d) == 0;
    return done;
}

Socket listen_tcp(std::uint16_t port, std::string* error) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        set_error(error, "socket");
        return invalid_socket;
    }
    sockaddr_in a {};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || listen(s, 4) != 0) {
        set_error(error, "bind/listen");
        closesocket(s);
        return invalid_socket;
    }
    return static_cast<Socket>(s);
}

std::uint16_t local_port(Socket s) {
    sockaddr_in a {};
    int len = sizeof a;
    if (getsockname(sock(s), reinterpret_cast<sockaddr*>(&a), &len) != 0) return 0;
    return ntohs(a.sin_port);
}

Socket accept_tcp(Socket listener, int timeout_ms, std::string* peer) {
    if (wait_readable(sock(listener), timeout_ms) != 1) return invalid_socket;
    sockaddr_in a {};
    int len = sizeof a;
    SOCKET c = accept(sock(listener), reinterpret_cast<sockaddr*>(&a), &len);
    if (c == INVALID_SOCKET) return invalid_socket;
    BOOL one = TRUE;
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
    if (peer) {
        char buf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &a.sin_addr, buf, sizeof buf);
        *peer = std::string(buf) + ":" + std::to_string(ntohs(a.sin_port));
    }
    return static_cast<Socket>(c);
}

Socket connect_tcp(const std::string& host, std::uint16_t port, std::string* error) {
    addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string service = std::to_string(port);
    if (getaddrinfo(host.c_str(), service.c_str(), &hints, &res) != 0 || res == nullptr) {
        set_error(error, "resolve");
        return invalid_socket;
    }
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET || connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
        set_error(error, "connect");
        if (s != INVALID_SOCKET) closesocket(s);
        freeaddrinfo(res);
        return invalid_socket;
    }
    freeaddrinfo(res);
    BOOL one = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
    return static_cast<Socket>(s);
}

bool send_all(Socket s, const char* data, std::size_t size) {
    while (size > 0) {
        const int chunk = size > 65536 ? 65536 : static_cast<int>(size);
        const int n = ::send(sock(s), data, chunk, 0);
        if (n <= 0) return false;
        data += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

long recv_some(Socket s, char* buffer, std::size_t size, int timeout_ms) {
    const int w = wait_readable(sock(s), timeout_ms);
    if (w == 0) return -2;
    if (w < 0) return -1;
    const int chunk = size > 65536 ? 65536 : static_cast<int>(size);
    const int n = ::recv(sock(s), buffer, chunk, 0);
    return n < 0 ? -1 : n;
}

void close_socket(Socket s) {
    if (s != invalid_socket) closesocket(sock(s));
}

}  // namespace vitals::platform::net
