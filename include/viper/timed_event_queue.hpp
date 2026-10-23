#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

namespace viper {

struct TimedEventMessage {
    std::int32_t due_tick{};
    std::uint32_t event_code{};
    std::vector<std::byte> payload;
};

struct TimedEventQueue {
    // Messages are appended at the back and consumed from the front, matching
    // the executable's newest-head/oldest-tail linked-list arrangement.
    std::deque<TimedEventMessage> pending;
};

struct TimedEventLatency {
    std::int32_t offset{};
};

struct TimedEventPollResult {
    bool consumed{};
    bool copied{};
    std::uint32_t event_code{};
    std::vector<std::byte> payload;
};

// Models the latency command in 0x4A3470 and setter 0x49C5A0. The caller has
// already parsed the integer after the command token; only [0, 1000) is stored
// and later added directly to the queue clock value.
[[nodiscard]] bool apply_timed_event_latency_command(
    TimedEventLatency& latency,
    std::int32_t parsed_value) noexcept;

// Models 0x49DB20. The due tick is current_tick + clock_offset with 32-bit
// wraparound; false means the queue-node allocator failed.
[[nodiscard]] bool enqueue_timed_event(
    TimedEventQueue& queue,
    std::span<const std::byte> payload,
    std::uint32_t event_code,
    std::int32_t current_tick,
    std::int32_t clock_offset);

// Models 0x49DBC0. A due event is always consumed; copied is false when its
// payload exceeds the caller's capacity, matching the original dequeue path.
[[nodiscard]] TimedEventPollResult poll_timed_event(
    TimedEventQueue& queue,
    std::int32_t current_tick,
    std::size_t output_capacity);

} // namespace viper
