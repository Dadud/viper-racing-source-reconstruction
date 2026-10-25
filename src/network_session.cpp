#include "viper/network_session.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace viper::net {
namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
void close_socket(Socket s) { if (s != invalid_socket) closesocket(s); }
void initialize_sockets() {
    static const bool initialized = [] { WSADATA data{}; return WSAStartup(MAKEWORD(2, 2), &data) == 0; }();
    if (!initialized) throw std::runtime_error("Winsock initialization failed");
}
int socket_error() { return WSAGetLastError(); }
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
void close_socket(Socket s) { if (s != invalid_socket) ::close(s); }
void initialize_sockets() {}
int socket_error() { return errno; }
#endif

void send_all(Socket s, const std::uint8_t* data, std::size_t size) {
    while (size) {
#ifdef _WIN32
        const int n = ::send(s, reinterpret_cast<const char*>(data), static_cast<int>(size), 0);
#else
        const auto n = ::send(s, data, size, 0);
#endif
        if (n <= 0) throw std::runtime_error("network send failed: " + std::to_string(socket_error()));
        data += n; size -= static_cast<std::size_t>(n);
    }
}
void receive_all(Socket s, std::uint8_t* data, std::size_t size) {
    while (size) {
#ifdef _WIN32
        const int n = ::recv(s, reinterpret_cast<char*>(data), static_cast<int>(size), 0);
#else
        const auto n = ::recv(s, data, size, 0);
#endif
        if (n <= 0) throw std::runtime_error("network connection closed or receive failed");
        data += n; size -= static_cast<std::size_t>(n);
    }
}
void put_u16(std::uint8_t* p, std::uint16_t v) { p[0]=static_cast<std::uint8_t>(v>>8); p[1]=static_cast<std::uint8_t>(v); }
void put_u32(std::uint8_t* p, std::uint32_t v) { p[0]=static_cast<std::uint8_t>(v>>24); p[1]=static_cast<std::uint8_t>(v>>16); p[2]=static_cast<std::uint8_t>(v>>8); p[3]=static_cast<std::uint8_t>(v); }
std::uint16_t get_u16(const std::uint8_t* p) { return static_cast<std::uint16_t>((p[0]<<8)|p[1]); }
std::uint32_t get_u32(const std::uint8_t* p) { return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3]; }

void send_handshake(Socket s, bool accept) {
    // 12 bytes: magic, protocol version, role/status, reserved.
    const std::array<std::uint8_t, 12> hello{'V','R','N','2',0,0,accept?std::uint8_t{1}:std::uint8_t{0},0,0,0,0,0};
    auto bytes = hello;
    put_u16(bytes.data()+4, protocol_version);
    send_all(s, bytes.data(), bytes.size());
}
void receive_handshake(Socket s, bool expect_accept) {
    std::array<std::uint8_t,12> hello{};
    receive_all(s, hello.data(), hello.size());
    if (std::memcmp(hello.data(), "VRN2", 4) || get_u16(hello.data()+4) != protocol_version || hello[6] != (expect_accept ? 1 : 0))
        throw std::runtime_error("incompatible multiplayer protocol handshake");
}
}

struct Session::Impl {
    Socket socket{invalid_socket};
    std::uint32_t last_received{};
    bool received_any{};
    ~Impl() { close_socket(socket); }
};

Session::Session() noexcept = default;
Session::Session(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Session::~Session() = default;
Session::Session(Session&&) noexcept = default;
Session& Session::operator=(Session&&) noexcept = default;

Session Session::host(std::uint16_t port) {
    initialize_sockets();
    Socket listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == invalid_socket) throw std::runtime_error("could not create host socket");
    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_ANY); address.sin_port=htons(port);
    if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(listener, 1) != 0) {
        close_socket(listener); throw std::runtime_error("could not bind/listen for multiplayer client");
    }
    Socket peer = ::accept(listener, nullptr, nullptr); close_socket(listener);
    if (peer == invalid_socket) throw std::runtime_error("could not accept multiplayer client");
    try { receive_handshake(peer, false); send_handshake(peer, true); }
    catch (...) { close_socket(peer); throw; }
    auto impl=std::make_unique<Impl>(); impl->socket=peer; return Session(std::move(impl));
}

Session Session::join(const std::string& host_name, std::uint16_t port) {
    initialize_sockets();
    addrinfo hints{}; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP;
    addrinfo* addresses=nullptr; const auto service=std::to_string(port);
    if (getaddrinfo(host_name.c_str(), service.c_str(), &hints, &addresses) != 0) throw std::runtime_error("could not resolve multiplayer host");
    Socket peer=invalid_socket;
    for (auto* a=addresses; a; a=a->ai_next) {
        peer=::socket(a->ai_family,a->ai_socktype,a->ai_protocol);
        if (peer != invalid_socket && ::connect(peer,a->ai_addr,static_cast<int>(a->ai_addrlen)) == 0) break;
        close_socket(peer); peer=invalid_socket;
    }
    freeaddrinfo(addresses);
    if (peer == invalid_socket) throw std::runtime_error("could not connect to multiplayer host");
    try { send_handshake(peer,false); receive_handshake(peer,true); }
    catch (...) { close_socket(peer); throw; }
    auto impl=std::make_unique<Impl>(); impl->socket=peer; return Session(std::move(impl));
}

void Session::send_state(std::uint32_t sequence, std::uint32_t tick, std::span<const std::uint8_t> payload) {
    if (!impl_) throw std::logic_error("multiplayer session is not connected");
    if (payload.size()>maximum_state_bytes) throw std::length_error("state snapshot exceeds protocol limit");
    // Frame: u32 body length, type=1, sequence, simulation tick, payload.
    std::array<std::uint8_t,13> header{}; put_u32(header.data(),static_cast<std::uint32_t>(9+payload.size()));
    header[4]=1; put_u32(header.data()+5,sequence); put_u32(header.data()+9,tick);
    send_all(impl_->socket,header.data(),header.size());
    if (!payload.empty()) send_all(impl_->socket,payload.data(),payload.size());
}

bool Session::receive_state(StateSnapshot& snapshot) {
    if (!impl_) throw std::logic_error("multiplayer session is not connected");
    std::array<std::uint8_t,4> length_bytes{}; receive_all(impl_->socket,length_bytes.data(),length_bytes.size());
    const auto length=get_u32(length_bytes.data());
    if (length<9 || length>9+maximum_state_bytes) throw std::runtime_error("invalid multiplayer frame length");
    std::vector<std::uint8_t> body(length); receive_all(impl_->socket,body.data(),body.size());
    if (body[0]!=1) throw std::runtime_error("unknown multiplayer frame type");
    const auto sequence=get_u32(body.data()+1);
    // RFC-style serial-number ordering without converting an out-of-range
    // unsigned value to a signed integer (whose result is implementation-defined).
    const auto sequence_delta = sequence - impl_->last_received;
    if (impl_->received_any &&
        (sequence_delta == 0 || sequence_delta >= (std::uint32_t{1} << 31))) return false;
    impl_->last_received=sequence; impl_->received_any=true;
    snapshot.sequence=sequence; snapshot.simulation_tick=get_u32(body.data()+5);
    snapshot.payload.assign(body.begin()+9,body.end()); return true;
}

Session::operator bool() const noexcept { return impl_ && impl_->socket!=invalid_socket; }
} // namespace viper::net
