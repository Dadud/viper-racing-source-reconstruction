#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace viper {

// The native sound manager reads a count followed by seven-float records.
// The values are kept positional until each field's meaning is confirmed.
struct EngineSoundSetupView {
    std::uint32_t band_count{};
    std::array<std::array<float, 7>, 7> bands{};
    std::span<const std::byte> opaque_tail;
};

[[nodiscard]] std::optional<EngineSoundSetupView> parse_engine_sound_setup(
    std::span<const std::byte> payload) noexcept;

// Pure reconstruction of 0x46C930. The native routine receives a pointer to
// one seven-float ESFX row and a control value. Its 0x4E9754 division-helper
// gate is external state; when set, the helper's result is not modeled here.
enum class EngineSoundBandBranch {
    outside_gate,
    central_ramp,
    upper_ramp,
    lower_ramp,
};

enum class EngineSoundBandStatus {
    evaluated,
    unresolved_division_helper,
    invalid_or_nonfinite_input,
};

struct EngineSoundBandEvaluation {
    EngineSoundBandStatus status{EngineSoundBandStatus::invalid_or_nonfinite_input};
    EngineSoundBandBranch branch{EngineSoundBandBranch::outside_gate};
    float value{};
};

struct EngineSoundBandInput {
    std::array<float, 7> band{};
    float control_value{};
    float outside_value{}; // native DAT_004C9674 (observed as 0.0)
    bool division_helper_gate_clear{}; // native DAT_004E9754 == 0
};

[[nodiscard]] EngineSoundBandEvaluation evaluate_engine_sound_band_46c930(
    const EngineSoundBandInput& input) noexcept;

// Relevant sound-object bytes/words touched by 0x46CA10. The caller supplies
// the object snapshot and all three native arguments; no vehicle/global state
// is read implicitly.
struct EngineSoundObjectState {
    float value_at_04{};
    float value_at_08{};
    bool flag_at_29{};
    bool flag_at_2a{};
    bool flag_at_2b{};
    bool flag_at_2c{};
    bool flag_at_2d{};
};

enum class EngineSoundUpdateStatus {
    updated,
    sound_disabled_for_frame,
    unresolved_division_helper,
    missing_sound_object,
    invalid_or_nonfinite_input,
};

struct EngineSoundUpdateInput {
    EngineSoundBandInput band_input{}; // includes native arg1: control value
    float object_scale{}; // native arg3, applied to 0x46C930 result
    std::optional<EngineSoundObjectState> sound_object; // native arg2 state
};

struct EngineSoundUpdateResult {
    EngineSoundUpdateStatus status{EngineSoundUpdateStatus::invalid_or_nonfinite_input};
    EngineSoundBandBranch branch{EngineSoundBandBranch::outside_gate};
    EngineSoundObjectState sound_object{};
};

[[nodiscard]] EngineSoundUpdateResult apply_engine_sound_update_46ca10(
    const EngineSoundUpdateInput& input) noexcept;

} // namespace viper
