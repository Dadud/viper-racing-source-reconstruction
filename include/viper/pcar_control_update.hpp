#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace viper {

// Raw fields used by 0x4330F0. Names preserve offsets because their game
// meanings have not been established.
struct PcarControlFields {
    float value_0x500{};
    float multiplier_0xdcc{};
    bool mode_0xbe4{};
    float output_0x630{};
    float output_0x7d4{};
    float output_0x978{};
    float output_0xb1c{};
};

// Reconstructs the finite-value control update in 0x4330F0:
// delta clamp [-0.24, 0.24], accumulated value clamp [-1, 1], then writes
// multiplier * value to one of two output pairs according to mode_0xbe4.
void update_pcar_control(PcarControlFields& fields, float requested_value) noexcept;

// These four wrappers are direct stores observed in 0x4331A0..0x4331D0.
struct PcarScalarFields {
    float value_0xde0{};
    float value_0xde4{};
    float value_0xde8{};
    float value_0xdec{};
};

struct PcarConstructorControls {
    // The constructor's default local values are enabled for these four
    // CONTROL settings; yaw control defaults to zero.
    bool traction_control{true};
    bool abs_braking{true};
    bool auto_shifting{true};
    bool auto_clutch{true};
    bool auto_clutch_0x534{true};
    std::int32_t yaw_control{};
    // 0x4330B0 writes +0x6B0 to four wheel objects; 0x4330D0 writes +0x6B8.
    std::array<std::uint32_t, 4> wheel_traction_0x6b0{};
    std::array<std::uint32_t, 4> wheel_abs_0x6b8{};
    std::optional<std::uint32_t> game_mode_0x4f4;
    bool auto_clutch_forced_by_auto_shift{};
};

// Models the settings-derived control defaults and the four-wheel direct
// stores in PCAR constructor 0x4381E0 / setters 0x4330B0 and 0x4330D0.
[[nodiscard]] PcarConstructorControls initialize_pcar_constructor_controls(
    std::optional<bool> traction_control,
    std::optional<bool> abs_braking,
    std::optional<bool> auto_shifting,
    std::optional<bool> auto_clutch,
    std::optional<std::int32_t> yaw_control,
    std::optional<std::uint32_t> game_mode_0x4f4) noexcept;

struct PcarCallbackTailFields {
    std::uint8_t value_0xc3c{};
    std::uint8_t gate_0xc3d{};
    // Embedded +0xBFC curve object's byte +0x41, read by 0x4402D0.
    std::uint8_t curve_gate_0x41{};
    float value_0xc7c{};
    std::uint32_t mode_0x4fc{};
    float value_0xde0{};
    float value_0xde4{};
    bool mode_0xbe4{};
    float value_0x508{};
};

struct PcarCallbackTailResult {
    bool called_curve_helper_0x4402d0{};
    bool set_byte_0xc3c{};
    bool reset_0xde0{};
    bool floored_0xde4{};
    bool called_value_helper_0x434b20{};
};

// Models the late 0x4384D0 helper branches after the shared-global caches.
[[nodiscard]] PcarCallbackTailResult advance_pcar_callback_tail(
    PcarCallbackTailFields& fields,
    float value_from_0x43ba10) noexcept;

void set_pcar_scalar_0xde0(PcarScalarFields& fields, float value) noexcept;
void set_pcar_scalar_0xde4(PcarScalarFields& fields, float value) noexcept;
void set_pcar_scalar_0xde8(PcarScalarFields& fields, float value) noexcept;
void set_pcar_scalar_0xdec(PcarScalarFields& fields, float value) noexcept;

// Selected shared callback globals; meanings remain unassigned. The overlapping
// data read by 0x43B990 is intentionally left as a separate raw input.
struct PcarCallbackGlobals {
    float value_0x509320{};
    float value_0x509344{};
    float value_0x509340{};
    float value_0x5093b8{};
    float value_0x509358{};
    float value_0x5093f4{};
    float value_0x5093c8{};
    std::uint32_t value_0x5093bc{};
    bool gate_0x509360{};
    bool gate_0x5093cc{};
    bool gate_0x5093f0{};
    bool gate_0x5093f8{};
    bool gate_0x5093fc{};
    bool gate_0x509420{};
    std::uint32_t value_0x509384{};
    std::uint32_t value_0x509388{};
    std::uint32_t value_0x5093a4{};
};

void set_pcar_callback_globals_0x43b880(PcarCallbackGlobals& globals,
                                         std::uint32_t value_0x509384,
                                         std::uint32_t value_0x509388,
                                         std::uint32_t value_0x5093a4) noexcept;

// Projects the initial scalar setter sequence in the 0x4384D0 callback.
// Later state-dependent callback branches may overwrite +0xDE0/+0xDE4.
void update_pcar_callback_scalar_prefix(
    PcarScalarFields& fields,
    const PcarCallbackGlobals& globals,
    float value_from_0x40d270) noexcept;

// Models 0x43B8C0, including its signed comparison of the input's raw bits.
float pcar_global_scale_0x43b8c0(float input,
                                 float global_0x509358,
                                 bool square_magnitude_0x5093fc) noexcept;

// Models the clamp derived from global 0x5093C8 in 0x43B950.
float pcar_callback_value_0x43b950(float global_0x5093c8) noexcept;

// Reproduces the two signed-bit comparisons written to the object's +4 dword.
std::uint32_t pcar_status_flags_0x4385f0(float value_0xde4,
                                         float value_0xc34) noexcept;

struct PcarTimestampUpdate {
    std::uint32_t timestamp_bits_0xecc{};
    bool request_backend_callback{};
};

// Models the timestamp latch at 0x438616; true indicates its backend call.
PcarTimestampUpdate update_pcar_timestamp(std::uint32_t current_timestamp_bits,
                                          bool enabled_0x509360,
                                          float current_time) noexcept;

struct PcarEffectFields {
    bool disabled_0xe3d{};
    std::uint32_t mode_0x4fc{};
    float selector_0x48{};
    std::array<float, 3> center_0x5c{};
    std::array<float, 3> direction_0x38{};
    std::array<float, 3> direction_0x44{};
    std::array<float, 3> direction_0x50{};
};

struct PcarEffectAccumulators {
    std::array<float, 3> force_0x24c{};
    std::array<float, 3> moment_0x258{};
};

// Models the branch and accumulator effects in 0x433250 / 0x43E6F0.
void accumulate_pcar_effect(PcarEffectAccumulators& accumulators,
                            const PcarEffectFields& fields) noexcept;

struct PcarNearbyEntity {
    std::array<float, 3> position_0x5c{};
    // True only when lookup is non-null, is not the source object, and its
    // virtual +0x30 query returns nonzero.
    bool passes_callback_filter{};
};

struct PcarNearbyEventState {
    std::uint32_t selector_bits_0x48{};
    std::array<float, 3> position_0x5c{};
    std::array<float, 3> motion_0x234{};
    std::array<float, 3> basis_0x38{};
    std::array<float, 3> basis_0x50{};
    bool event_enabled_0xeec{};
    float current_time{};
    float prior_event_time_0xee8{};
    std::uint32_t event_state_0xed0{};
    std::uint32_t prior_event_state_0xed4{};
    bool target_0xed8_present{};
    bool target_0xedc_present{};
    bool target_0xee0_present{};
    bool target_0xee4_present{};
};

enum class PcarNearbyCue : std::uint8_t {
    clear = 0,
    left = 1,
    right = 2,
    both = 3,
};

struct PcarNearbyEventResult {
    // True only when the scan reaches the changed-state notification branch.
    bool state_changed{};
    std::uint32_t event_state{};
    std::uint32_t notification_target_offset{};
    bool notification_target_present{};
    // Meaningful only when state_changed is true.
    PcarNearbyCue cue{PcarNearbyCue::clear};
};

// Models the 16-entry nearby-object scan and state-transition dispatch near
// the end of the PCAR callback (0x43879E..0x4389B4).
PcarNearbyEventResult update_pcar_nearby_events(
    PcarNearbyEventState& state,
    std::span<const PcarNearbyEntity> candidates) noexcept;

// Writes the changed-state cue pattern to its selected target object's bytes
// +0x29..+0x2B, when the scan found a present target.
[[nodiscard]] bool apply_pcar_nearby_notification(
    std::span<std::byte> target_object,
    const PcarNearbyEventResult& result) noexcept;

struct PcarStepRecordObservation {
    std::uint32_t backend_value_0x50{};
    std::uint32_t backend_value_0x48{};
    std::uint32_t object_value_0xddc{};
    std::uint32_t object_value_0xea4{};
    std::uint32_t object_value_0xea8{};
};

inline constexpr std::size_t pcar_step_record_bytes = 0x14;
inline constexpr std::uint32_t pcar_step_record_global_address = 0x00507db0U;

// Writes the five consecutive DWORDs emitted after 0x431150 into the global
// record returned by 0x422890. Unnamed fields and backend results stay raw.
[[nodiscard]] bool write_pcar_step_record(
    std::span<std::byte> destination,
    const PcarStepRecordObservation& observation) noexcept;

struct PcarCallbackPostIntegratorObservation {
    bool integrator_reached{};
    PcarStepRecordObservation step_record{};
    std::span<const PcarNearbyEntity> nearby_candidates{};
    // Caller-selected object addressed by the event result's target offset.
    std::span<std::byte> notification_target_object{};
};

struct PcarCallbackPostIntegratorResult {
    bool reached{};
    bool step_record_written{};
    PcarNearbyEventResult nearby_events{};
    bool nearby_notification_written{};
};

// Composes the 0x43872F post-integrator record stores followed by the nearby
// event scan, gated by reachability of 0x431150.
[[nodiscard]] PcarCallbackPostIntegratorResult
advance_pcar_callback_post_integrator(
    std::span<std::byte> step_record_destination,
    PcarNearbyEventState& nearby_state,
    const PcarCallbackPostIntegratorObservation& observation) noexcept;

} // namespace viper
