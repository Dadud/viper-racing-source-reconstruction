#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace viper {

// Numeric fields consumed by 0x4404A0/0x4404D0. Their game-level meanings are
// unknown, so the original byte offsets are retained in the names.
struct UpdateCurveFields {
    float subtract_scale_0x14{};
    float gate_value_0x34{};
    float blend_value_0x38{};
    float cutoff_0x3c{};
    std::uint8_t mode_0x40{};
    std::uint8_t mode_0x41{};
    float quadratic_0x44{};
    float linear_0x48{};
    float intercept_0x4c{};
    float coefficient_0x50{};
    float offset_0x54{};
    float penalty_0x58{};
};

struct ConfiguredUpdateCurve {
    std::uintptr_t opaque_0x04{};
    float value_0x08{};
    float reciprocal_0x0c{};
    float getter_value_0x10{};
    float derived_value_0x30{};
    std::int32_t selection_gate_0x68{0x43910000};
    UpdateCurveFields scoring{};
};

// Models 0x440100 followed by 0x4401D0 for the fields used by the recovered
// evaluator. The six input coefficients map in order to offsets +0x44..+0x58.
[[nodiscard]] ConfiguredUpdateCurve configure_update_curve(
    std::uintptr_t parameter_1,
    float parameter_2,
    float parameter_3,
    const std::array<float, 6>& parameter_4,
    float parameter_5,
    float parameter_6) noexcept;

// Reconstructs the piecewise arithmetic observed at 0x4404A0 for ordinary
// finite inputs. It does not infer the curve's application-level meaning.
[[nodiscard]] float evaluate_update_curve(
    const UpdateCurveFields& curve,
    float x,
    float scale) noexcept;

// Numeric fields consumed by 0x43FDD0 in the embedded object at parent
// +0xC98. These are raw offsets; no semantic labels are inferred.
struct EmbeddedSelectionFields {
    float denominator_0x18{};
    int floor_index_0x1c{};
    int entry_count_0x20{};
    std::span<const float> entries_0x2c{};
    int current_index_0x24{};
    float base_value_0x28{};
    bool preserve_positive_selection_0x68{true};
};

// Models the numeric selection path. Its virtual +0 getter returns a value
// that is discarded, so it has no state effect on the modeled fields.
// Returns the value written to +0x24, or the prior value when preserved.
[[nodiscard]] int update_embedded_selection(
    const EmbeddedSelectionFields& selection,
    const UpdateCurveFields& curve) noexcept;

} // namespace viper
