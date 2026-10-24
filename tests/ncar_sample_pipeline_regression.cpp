#include "viper/ncar_sample_pipeline.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main()
{
    try {
        viper::NcarSamplePipelineState state{};
        state.producer_body.orientation_0x38 = {
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F};
        state.producer_body.position_0x5c = {1.0F, 2.0F, 3.0F};
        state.producer_body.value_0xd70 = 4000.0F;
        state.producer_body.value_0x500 = 0.0F;
        state.producer_body.value_0xde4 = 0.5F;
        state.producer_body.value_0xd64 = 3U;
        state.expanded_global_record.raw_bytes[0] = std::byte{0xA5};
        state.expanded_global_record.time_bits_0x00 = 0xA5U;
        state.expanded_global_record.raw_bytes[0x10] = std::byte{0x5A};
        state.expanded_global_record.value_bits_0x10 = 0x5AU;

        viper::NcarSamplePipelineObservation observation{};
        observation.current_time = 1.0F;
        observation.queue_clock = 1000;
        observation.tick_origin = 1000U;
        observation.seconds_origin = 1.0F;
        auto result = viper::advance_ncar_sample_pipeline(state, observation);
        require(result.producer_branch_reached && result.producer_capture_succeeded &&
                result.callback.enqueue_succeeded &&
                result.callback.copied_payload_has_sample_size &&
                result.callback.sample_update.accepted,
            "timed producer record must pack, expand, enqueue, and enter the ring");
        require(state.expanded_global_record.raw_bytes[0] == std::byte{0xA5} &&
                state.expanded_global_record.raw_bytes[0x10] == std::byte{0x5A} &&
                state.sample_buffer.bytes[0x10] == std::byte{0x5A},
            "expansion and ring copy must preserve the native unwritten +0x10 field");
        require(state.sample_buffer.bytes[0x00] ==
                    static_cast<std::byte>(std::bit_cast<std::uint32_t>(1.0F) & 0xffU) &&
                state.sample_buffer.bytes[0x1c] == std::byte{0x00},
            "accepted ring record must carry callback time and the captured position");

        observation.current_time = 1.01F;
        auto gated = viper::advance_ncar_sample_pipeline(state, observation);
        require(!gated.producer_branch_reached && !gated.producer_capture_succeeded,
            "producer capture must respect the strict callback interval");

        for (std::size_t record = 1U; record <= 3U; ++record) {
            observation.current_time = 1.0F + static_cast<float>(record) * 0.1F;
            observation.queue_clock = static_cast<std::int32_t>(
                std::lround(observation.current_time * 1000.0F));
            state.producer_body.position_0x5c[0] += 1.0F;
            const auto next = viper::advance_ncar_sample_pipeline(state, observation);
            if (record < 3U) {
                require(next.callback.sample_update.accepted &&
                        !next.sample_dynamics_advanced,
                    "the producer must fill the three-slot window before dynamics run");
            } else {
                require(next.callback.sample_update.accepted &&
                        next.callback.sample_update.run_post_accept_helpers &&
                        next.sample_dynamics_advanced &&
                        std::all_of(next.sample_dynamics.current_derivative.values.begin(),
                            next.sample_dynamics.current_derivative.values.end(),
                            [](const float value) { return std::isfinite(value); }),
                    "the fourth accepted sample must advance finite three-sample dynamics");
            }
        }

        observation.event_processor_active_0x49be40 = true;
        observation.current_time = 2.0F;
        const float last_time = state.callback_queue.last_payload_time_0xf30;
        auto paused = viper::advance_ncar_sample_pipeline(state, observation);
        require(!paused.producer_branch_reached && !paused.callback.dequeue_attempted &&
                state.callback_queue.last_payload_time_0xf30 == last_time,
            "active event processing must suppress producer and queue work");

        std::cout << "NCAR sample pipeline regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
