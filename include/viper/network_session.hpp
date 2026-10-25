#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace viper::net {

inline constexpr std::uint16_t protocol_version = 1;
inline constexpr std::size_t maximum_state_bytes = 64 * 1024;

struct StateSnapshot {
    std::uint32_t sequence{};
    std::uint32_t simulation_tick{};
    std::vector<std::uint8_t> payload;
};

// A connected, reliable host/client session. State payloads are opaque to the
// transport; callers own the schema and should send complete snapshots.
class Session {
public:
    Session() noexcept;
    ~Session();
    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    static Session host(std::uint16_t port);
    static Session join(const std::string& host, std::uint16_t port);

    void send_state(std::uint32_t sequence, std::uint32_t simulation_tick,
                    std::span<const std::uint8_t> payload);
    // Returns false only for a stale/duplicate sequence. Malformed frames throw.
    bool receive_state(StateSnapshot& snapshot);
    explicit operator bool() const noexcept;

private:
    struct Impl;
    explicit Session(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace viper::net
