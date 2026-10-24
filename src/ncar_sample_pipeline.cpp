#include "viper/ncar_sample_pipeline.hpp"

#include <bit>
#include <span>

namespace viper {

NcarSamplePipelineResult advance_ncar_sample_pipeline(
    NcarSamplePipelineState& state,
    const NcarSamplePipelineObservation& observation)
{
    NcarSamplePipelineResult result{};

    if (!observation.event_processor_active_0x49be40) {
        constexpr auto payload_interval = std::bit_cast<float>(
            std::uint32_t{0x3D888889U});
        if (static_cast<long double>(observation.current_time) -
            static_cast<long double>(state.callback_queue.last_payload_time_0xf30) >
            static_cast<long double>(payload_interval)) {
            result.producer_branch_reached = true;
            result.callback.enqueue_branch_reached = true;
            // The binary commits +0xF30 before entering the producer update.
            state.callback_queue.last_payload_time_0xf30 = observation.current_time;
            const auto capture = capture_ncar_sample_record(
                state.producer_scratch_record, state.producer_body,
                observation.current_time,
                observation.fdiv_compatibility_enabled);
            result.producer_capture_succeeded = capture.captured;
            if (capture.captured) {
                result.packed_payload = encode_ncar_packed_payload24(
                    state.producer_scratch_record, observation.tick_origin,
                    observation.seconds_origin,
                    observation.fdiv_compatibility_enabled);
                result.expansion = decode_ncar_packed_payload24(
                    std::span<const std::byte, 0x18>{result.packed_payload.bytes},
                    static_cast<std::uint32_t>(observation.queue_clock),
                    observation.tick_origin, observation.seconds_origin,
                    state.expanded_global_record,
                    observation.fdiv_compatibility_enabled);
                result.callback.enqueue_succeeded = enqueue_timed_event(
                    state.event_queue,
                    state.expanded_global_record.raw_bytes,
                    0U,
                    observation.queue_clock,
                    observation.queue_clock_offset);
            }
        }

        result.callback.dequeue_attempted = true;
        result.callback.dequeue = poll_timed_event(
            state.event_queue, observation.queue_clock, ncar_sample_record_bytes);
        result.callback.copied_payload_has_sample_size =
            result.callback.dequeue.consumed && result.callback.dequeue.copied &&
            result.callback.dequeue.payload.size() == ncar_sample_record_bytes;
        if (result.callback.copied_payload_has_sample_size) {
            const std::span<const std::byte, ncar_sample_record_bytes> incoming{
                result.callback.dequeue.payload.data(), ncar_sample_record_bytes};
            result.callback.sample_update = update_ncar_sample_buffer(
                state.sample_buffer, incoming, observation.current_time);
            if (result.callback.sample_update.accepted &&
                result.callback.sample_update.run_post_accept_helpers) {
                const auto sample_window = decode_ncar_sample_window(
                    state.sample_buffer.bytes);
                result.sample_dynamics = advance_ncar_sample_dynamics(
                    state.sample_dynamics, sample_window,
                    observation.fdiv_compatibility_enabled);
                result.sample_dynamics_advanced = true;
            }
        }
    }
    return result;
}

} // namespace viper
