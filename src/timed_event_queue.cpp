#include "viper/timed_event_queue.hpp"

#include <bit>
#include <new>
#include <utility>

namespace viper {

bool apply_timed_event_latency_command(TimedEventLatency& latency,
                                       const std::int32_t parsed_value) noexcept
{
    if (parsed_value < 0 || parsed_value >= 1000) {
        return false;
    }
    latency.offset = parsed_value;
    return true;
}

bool enqueue_timed_event(
    TimedEventQueue& queue,
    const std::span<const std::byte> payload,
    const std::uint32_t event_code,
    const std::int32_t current_tick,
    const std::int32_t clock_offset)
{
    const auto now_bits = std::bit_cast<std::uint32_t>(current_tick);
    const auto offset_bits = std::bit_cast<std::uint32_t>(clock_offset);
    const auto due_tick = std::bit_cast<std::int32_t>(now_bits + offset_bits);
    try {
        TimedEventMessage message{};
        message.due_tick = due_tick;
        message.event_code = event_code;
        message.payload.assign(payload.begin(), payload.end());
        queue.pending.push_back(std::move(message));
    } catch (const std::bad_alloc&) {
        return false;
    }
    return true;
}

TimedEventPollResult poll_timed_event(
    TimedEventQueue& queue,
    const std::int32_t current_tick,
    const std::size_t output_capacity)
{
    if (queue.pending.empty() || queue.pending.front().due_tick > current_tick) {
        return {};
    }

    TimedEventPollResult result{};
    result.consumed = true;
    TimedEventMessage message = std::move(queue.pending.front());
    queue.pending.pop_front();
    if (message.payload.size() > output_capacity) {
        return result;
    }

    result.copied = true;
    result.event_code = message.event_code;
    result.payload = std::move(message.payload);
    return result;
}

} // namespace viper
