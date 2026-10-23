#include "viper/update_curve.hpp"

namespace viper {

ConfiguredUpdateCurve configure_update_curve(
    const std::uintptr_t parameter_1,
    const float parameter_2,
    const float parameter_3,
    const std::array<float, 6>& parameter_4,
    const float parameter_5,
    const float parameter_6) noexcept
{
    ConfiguredUpdateCurve configured{};
    configured.opaque_0x04 = parameter_1;
    configured.value_0x08 = parameter_2;
    configured.reciprocal_0x0c = 1.0F / parameter_2;
    configured.getter_value_0x10 = 1000.0F;

    auto& curve = configured.scoring;
    curve.subtract_scale_0x14 = parameter_3;
    curve.gate_value_0x34 = parameter_6;
    curve.blend_value_0x38 = 0.0F;
    curve.mode_0x40 = 0;
    curve.mode_0x41 = 0;
    curve.cutoff_0x3c = parameter_5;
    curve.quadratic_0x44 = parameter_4[0];
    curve.linear_0x48 = parameter_4[1];
    curve.intercept_0x4c = parameter_4[2];
    curve.coefficient_0x50 = parameter_4[3];
    curve.offset_0x54 = parameter_4[4];
    curve.penalty_0x58 = parameter_4[5];

    float curve_value = curve.linear_0x48 * parameter_6
        + (parameter_6 * parameter_6 * curve.quadratic_0x44
            - (parameter_6 > curve.offset_0x54
                ? (parameter_6 - curve.offset_0x54) * curve.penalty_0x58
                : 0.0F))
        + curve.intercept_0x4c;
    const float denominator_base = parameter_6 > 1000.0F ? parameter_6 : 1000.0F;
    const float denominator = (parameter_5 / denominator_base) * curve_value;
    configured.derived_value_0x30 = parameter_6 / denominator * parameter_3;
    return configured;
}

float evaluate_update_curve(
    const UpdateCurveFields& curve,
    const float x,
    const float scale) noexcept
{
    float response = 0.0F;
    if (curve.mode_0x41 == 0) {
        if (x < curve.cutoff_0x3c) {
            float distance = x - curve.offset_0x54;
            if (distance <= 0.0F) {
                distance = 0.0F;
            }
            response = (curve.linear_0x48 * x
                + (x * x * curve.quadratic_0x44 - distance * curve.penalty_0x58)
                + curve.intercept_0x4c) * scale;
        }
    } else if (curve.mode_0x40 != 0) {
        response = 80.0F;
    }

    return response - curve.subtract_scale_0x14 * x;
}

int update_embedded_selection(
    const EmbeddedSelectionFields& selection,
    const UpdateCurveFields& curve) noexcept
{
    if (selection.floor_index_0x1c <= 0 && selection.current_index_0x24 <= 0) {
        return 1;
    }
    if (selection.current_index_0x24 != selection.floor_index_0x1c) {
        return selection.current_index_0x24;
    }
    if (selection.denominator_0x18 <= 1.1920928955078125e-7F
        && selection.denominator_0x18 >= -1.1920928955078125e-7F) {
        return 1;
    }

    const float base = selection.base_value_0x28 / selection.denominator_0x18;
    if (curve.gate_value_0x34 <= selection.base_value_0x28) {
        return 1;
    }

    float blend = curve.blend_value_0x38;
    if (blend > 1.0F) {
        blend = 1.0F;
    }
    const float step = (blend * 500.0F + 500.0F)
        * curve.cutoff_0x3c * 0.00011764706141548231F;
    const float threshold = curve.cutoff_0x3c - 200.0F;

    float best_score = 0.0F;
    int best_index = selection.floor_index_0x1c;
    for (std::size_t i = 0;
         i < selection.entries_0x2c.size()
             && i < static_cast<std::size_t>(selection.entry_count_0x20 > 0
                    ? selection.entry_count_0x20 : 0);
         ++i) {
        const int one_based_index = static_cast<int>(i + 1);
        float x = base;
        if (selection.floor_index_0x1c > one_based_index) {
            x += step;
        }
        const float entry = selection.entries_0x2c[i];
        const float candidate = entry * x;
        float score = evaluate_update_curve(curve, candidate, 1.0F);
        if (candidate > threshold) {
            score = 0.0F;
        }
        if (score >= best_score) {
            best_score = score;
            best_index = one_based_index;
        }
    }

    if (selection.preserve_positive_selection_0x68 && best_index > 0) {
        return selection.current_index_0x24;
    }
    return best_index;
}

} // namespace viper
