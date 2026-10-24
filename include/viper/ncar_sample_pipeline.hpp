#pragma once

#include "viper/ncar_state.hpp"
#include "viper/timed_event_queue.hpp"

#include <cstdint>

namespace viper {

// Persistent state for the observed producer -> packed payload -> expanded
// callback record -> timed queue -> three-sample ring path. The NCAR body is
// updated by the runtime before each call, matching the preceding 0x420930
// effect which is not part of this sample transport model.
struct NcarSamplePipelineState {
    NcarSampleProducerObjectState producer_body{};
    NcarSampleRecord producer_scratch_record{};
    NcarSampleRecord expanded_global_record{};
    NcarSampleBuffer sample_buffer{};
    NcarSampleDynamicsState sample_dynamics{};
    NcarCallbackQueueState callback_queue{};
    TimedEventQueue event_queue{};
};

struct NcarSamplePipelineObservation {
    float current_time{};
    bool event_processor_active_0x49be40{};
    std::int32_t queue_clock{};
    std::int32_t queue_clock_offset{};
    std::uint32_t tick_origin{};
    float seconds_origin{};
    bool fdiv_compatibility_enabled{};
};

struct NcarSamplePipelineResult {
    bool producer_branch_reached{};
    bool producer_capture_succeeded{};
    NcarPackedPayload24 packed_payload{};
    NcarPackedPayloadApplyResult expansion{};
    NcarCallbackQueueResult callback{};
    bool sample_dynamics_advanced{};
    NcarSampleDynamicsResult sample_dynamics{};
};

// Advances the sample transport when the native callback runs. On the strict
// ~1/15-second producer interval it captures the already-updated body,
// encodes 24 bytes, expands those bytes over the persistent global 62-byte
// record (preserving fields the native expansion does not write), and queues
// that record. It then polls and applies an exact-size payload to the ring and,
// when accepted update mode is greater than 2, advances 0x439520 state.
[[nodiscard]] NcarSamplePipelineResult advance_ncar_sample_pipeline(
    NcarSamplePipelineState& state,
    const NcarSamplePipelineObservation& observation);

} // namespace viper
