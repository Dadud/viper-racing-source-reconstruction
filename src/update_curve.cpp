#include "viper/update_curve.hpp"

#include "viper/legacy_fdiv.hpp"

namespace viper {

ConfiguredUpdateCurve configure_update_curve(
    const std::uintptr_t parameter_1,
    const float parameter_2,
    const float parameter_3,
    const std::array<float, 6>& parameter_4,
    const float parameter_5,
    const float parameter_6,
    const bool fdiv_compatibility_enabled) noexcept
{
    ConfiguredUpdateCurve configured{};
    configured.opaque_0x04 = parameter_1;
    configured.value_0x08 = parameter_2;
    configured.reciprocal_0x0c = static_cast<float>(
        divide_with_vc98_fdiv_compatibility(
            1.0L, parameter_2, fdiv_compatibility_enabled));
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

    const auto profile_value = static_cast<long double>(parameter_6);
    auto delayed_distance = profile_value
        - static_cast<long double>(curve.offset_0x54);
    if (delayed_distance <= 0.0L) {
        delayed_distance = 0.0L;
    }
    const auto curve_value =
        static_cast<long double>(curve.linear_0x48) * profile_value
        + (profile_value * profile_value * curve.quadratic_0x44
            - delayed_distance * curve.penalty_0x58)
        + curve.intercept_0x4c;
    auto denominator_base = profile_value;
    if (profile_value <= 1000.0L) {
        denominator_base = 1000.0L;
    }
    const auto normalized_cutoff = divide_with_vc98_fdiv_compatibility(
        parameter_5, denominator_base, fdiv_compatibility_enabled);
    configured.derived_value_0x30 = static_cast<float>(
        divide_with_vc98_fdiv_compatibility(
            profile_value, normalized_cutoff * curve_value,
            fdiv_compatibility_enabled)
        * static_cast<long double>(parameter_3));
    return configured;
}

ConfiguredUpdateCurve configure_acar_update_curve(
    const std::uintptr_t parent_object,
    const AcarCurveSourceRecord& record,
    const bool fdiv_compatibility_enabled) noexcept
{
    constexpr auto torque_scale =
        static_cast<long double>(1.3515152F);
    const auto scaled_parameter_4 =
        static_cast<long double>(record.value_0x68) * torque_scale;
    const auto torque_at_scale = static_cast<float>(
        static_cast<long double>(record.value_0x84) * torque_scale);
    const auto ratio = divide_with_vc98_fdiv_compatibility(
        record.value_0x6c, record.value_0x74,
        fdiv_compatibility_enabled);
    const auto horsepower_term = ratio * 5252.0L * torque_scale;
    const auto rpm_delta = static_cast<long double>(record.value_0x74)
        - static_cast<long double>(record.value_0x70);
    const auto quadratic = divide_with_vc98_fdiv_compatibility(
        horsepower_term - scaled_parameter_4, rpm_delta * rpm_delta,
        fdiv_compatibility_enabled);
    const std::array<float, 6> coefficients{
        static_cast<float>(quadratic),
        static_cast<float>(static_cast<long double>(record.value_0x70)
            * quadratic * -2.0L + torque_at_scale),
        static_cast<float>(static_cast<long double>(record.value_0x70)
            * record.value_0x70 * quadratic + scaled_parameter_4),
        torque_at_scale,
        record.value_0x74,
        static_cast<float>(horsepower_term
            * static_cast<long double>(0.000125F))};
    const auto value_0x08 = static_cast<float>(
        static_cast<long double>(record.value_0x80)
        * static_cast<long double>(0.042279866F));
    return configure_update_curve(parent_object, value_0x08,
        torque_at_scale, coefficients, record.value_0x7c, record.value_0x78,
        fdiv_compatibility_enabled);
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
