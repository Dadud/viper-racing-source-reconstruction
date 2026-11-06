#include "viper/update_curve.hpp"

#include "viper/legacy_fdiv.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>

namespace viper {

std::optional<AcarCurveSourceRecord> read_car_curve_source_record(
    const std::span<const std::byte> runtime_record) noexcept
{
    constexpr std::size_t record_size = 0x1e4U;
    constexpr std::uint32_t pcar_tag = 0x50434152U;
    constexpr std::uint32_t acar_tag = 0x41434152U;
    if (runtime_record.size() != record_size) return std::nullopt;
    const auto read_u32 = [runtime_record](const std::size_t offset) noexcept {
        return std::to_integer<std::uint32_t>(runtime_record[offset])
            | (std::to_integer<std::uint32_t>(runtime_record[offset + 1U]) << 8U)
            | (std::to_integer<std::uint32_t>(runtime_record[offset + 2U]) << 16U)
            | (std::to_integer<std::uint32_t>(runtime_record[offset + 3U]) << 24U);
    };
    const auto tag = read_u32(0U);
    if ((tag != pcar_tag && tag != acar_tag) || read_u32(4U) != record_size)
        return std::nullopt;
    const auto read_float = [&read_u32](const std::size_t offset) noexcept {
        return std::bit_cast<float>(read_u32(offset));
    };
    return AcarCurveSourceRecord{
        read_float(0x68U), read_float(0x6cU), read_float(0x70U),
        read_float(0x74U), read_float(0x78U), read_float(0x7cU),
        read_float(0x80U), read_float(0x84U)};
}

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

std::array<ConfiguredUpdateCurveObjectWrite, 16>
make_configured_update_curve_object_writes(
    const ConfiguredUpdateCurve& configured,
    const std::uint32_t parent_subobject_offset) noexcept
{
    const auto float_bits = [](const float value) noexcept {
        return std::bit_cast<std::uint32_t>(value);
    };
    const auto field = [parent_subobject_offset](const std::uint32_t offset,
                            const std::uint8_t width,
                            const std::uint32_t value_bits) noexcept {
        return ConfiguredUpdateCurveObjectWrite{
            parent_subobject_offset + offset, width, value_bits};
    };
    const auto& curve = configured.scoring;
    return {{
        field(0x08U, 4U, float_bits(configured.value_0x08)),
        field(0x0cU, 4U, float_bits(configured.reciprocal_0x0c)),
        field(0x10U, 4U, float_bits(configured.getter_value_0x10)),
        field(0x14U, 4U, float_bits(curve.subtract_scale_0x14)),
        field(0x30U, 4U, float_bits(configured.derived_value_0x30)),
        field(0x34U, 4U, float_bits(curve.gate_value_0x34)),
        field(0x38U, 4U, float_bits(curve.blend_value_0x38)),
        field(0x3cU, 4U, float_bits(curve.cutoff_0x3c)),
        field(0x40U, 1U, curve.mode_0x40),
        field(0x41U, 1U, curve.mode_0x41),
        field(0x44U, 4U, float_bits(curve.quadratic_0x44)),
        field(0x48U, 4U, float_bits(curve.linear_0x48)),
        field(0x4cU, 4U, float_bits(curve.intercept_0x4c)),
        field(0x50U, 4U, float_bits(curve.coefficient_0x50)),
        field(0x54U, 4U, float_bits(curve.offset_0x54)),
        field(0x58U, 4U, float_bits(curve.penalty_0x58)),
    }};
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

std::optional<UpdateCurveSetterPrefixResult> project_update_curve_setter_prefix(
    const UpdateCurveSetterPrefixInput& input) noexcept
{
    const auto& curve = input.curve;
    const std::array<float, 11> values{input.getter_value_0x10,
        input.control_value_0x2c, input.derived_value_0x30,
        input.setter_argument, curve.blend_value_0x38, curve.cutoff_0x3c,
        curve.quadratic_0x44, curve.linear_0x48, curve.intercept_0x4c,
        curve.offset_0x54, curve.penalty_0x58};
    if (!std::all_of(values.begin(), values.end(),
            [](const float value) { return std::isfinite(value); })) return std::nullopt;
    constexpr float retained = std::bit_cast<float>(0x3f333333U);
    constexpr float incoming = std::bit_cast<float>(0x3e99999aU);
    const auto blended = static_cast<float>(
        static_cast<long double>(curve.blend_value_0x38) * retained +
        static_cast<long double>(input.control_value_0x2c) * incoming);
    const auto base = (1.0L - static_cast<long double>(input.derived_value_0x30)) *
        input.control_value_0x2c + input.derived_value_0x30;
    const auto denominator = (std::max)(1000.0L,
        static_cast<long double>(input.getter_value_0x10));
    const auto scale = static_cast<float>((std::min)(1.0L,
        base * static_cast<long double>(curve.cutoff_0x3c) / denominator));
    float raw = 0.0F;
    if (curve.mode_0x41 == 0U && input.getter_value_0x10 < curve.cutoff_0x3c) {
        const long double x = input.getter_value_0x10;
        const long double distance = (std::max)(0.0L, x - curve.offset_0x54);
        raw = static_cast<float>((static_cast<long double>(curve.linear_0x48) * x +
            (x * x * curve.quadratic_0x44 - distance * curve.penalty_0x58) +
            curve.intercept_0x4c) * scale);
    } else if (curve.mode_0x41 != 0U && curve.mode_0x40 != 0U) {
        raw = 80.0F;
    }
    const auto argument = static_cast<float>(
        static_cast<long double>(raw) + input.setter_argument);
    if (!std::isfinite(blended) || !std::isfinite(scale) ||
        !std::isfinite(raw) || !std::isfinite(argument)) return std::nullopt;
    return UpdateCurveSetterPrefixResult{blended, scale, raw, argument};
}

std::optional<UpdateCurveSetterTailResult> project_update_curve_setter_tail(
    const UpdateCurveSetterTailInput& input) noexcept
{
    const std::array<float, 7> values{input.previous_getter_0x10,
        input.raw_curve_output, input.damping_0x14, input.value_0x24,
        input.accumulator_0x28, input.value_0x68, input.accumulator_0x6c};
    if (!std::all_of(values.begin(), values.end(),
            [](const float value) { return std::isfinite(value); })) return std::nullopt;
    constexpr float raw_weight = std::bit_cast<float>(0x3dcccccdU);
    constexpr float angular_scale = std::bit_cast<float>(0x3dd67750U);
    constexpr float timestep = std::bit_cast<float>(0x3c83126fU);
    constexpr float threshold = std::bit_cast<float>(0x43b6aaabU);
    constexpr float coupling_scale = std::bit_cast<float>(0x3e3851ecU);
    constexpr float coupling_max = std::bit_cast<float>(0x461c4000U);

    const long double getter = input.previous_getter_0x10;
    const auto intermediate = static_cast<float>(input.accumulator_0x28 +
        (getter * input.damping_0x14 +
            static_cast<long double>(input.raw_curve_output) * raw_weight) *
        getter * angular_scale * timestep);
    // 0x4403C6 stores this product before the upper clamp and multiply.
    const float fraction = (std::clamp)(static_cast<float>(
        (static_cast<long double>(input.value_0x24) - threshold) * coupling_scale),
        0.0F, 1.0F);
    const auto coupling = static_cast<float>(fraction * coupling_max);
    const long double transfer =
        (static_cast<long double>(input.value_0x68) - input.value_0x24) *
        coupling * timestep;
    UpdateCurveSetterTailResult result{};
    result.accumulator_0x28 = static_cast<float>(intermediate + transfer);
    result.accumulator_0x6c = static_cast<float>(input.accumulator_0x6c - transfer);
    result.coupling_0x74 = coupling;
    result.mode_0x40 = input.mode_0x40;
    result.mode_0x41 = input.mode_0x41;
    if (std::bit_cast<std::int32_t>(input.previous_getter_0x10) > 0x43c80000) {
        result.mode_0x40 = 0U;
        result.mode_0x41 = 0U;
    } else {
        result.mode_0x41 = 1U;
    }
    if (!std::isfinite(result.accumulator_0x28) ||
        !std::isfinite(result.accumulator_0x6c) ||
        !std::isfinite(result.coupling_0x74)) return std::nullopt;
    return result;
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
