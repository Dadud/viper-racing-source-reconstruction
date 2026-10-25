#include "viper/vehicle_setup.hpp"

#include <bit>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>

namespace viper {

std::optional<HumanCarSetupFile> parse_human_car_setup_file(
    const std::span<const std::byte> payload) noexcept
{
    if (payload.size() != HumanCarSetupFile::file_size) {
        return std::nullopt;
    }

    HumanCarSetupFile result{};
    std::copy_n(payload.begin(), result.active_setup.size(),
        result.active_setup.begin());
    constexpr std::size_t version_offset = HumanCarSetupFile::active_setup_size;
    result.version =
        static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload[version_offset])) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload[version_offset + 1U])) << 8U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload[version_offset + 2U])) << 16U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(payload[version_offset + 3U])) << 24U);
    result.version_warning = result.version != 1U;
    return result;
}

std::optional<std::uint32_t> world_car_object_tag(
    const std::uint32_t record_variant) noexcept
{
    switch (record_variant) {
    case 0:
        return 0x50434152U; // PCAR
    case 1:
        return 0x41434152U; // ACAR
    case 2:
        return 0x4E434152U; // NCAR
    case 3:
        return 0x47434152U; // GCAR
    default:
        return std::nullopt;
    }
}

std::optional<WorldCarSceneRecordIdentity> make_world_car_scene_record_identity(
    const std::uint32_t record_variant,
    const std::int32_t car_record_ordinal) noexcept
{
    const auto tag = world_car_object_tag(record_variant);
    if (!tag) {
        return std::nullopt;
    }
    return WorldCarSceneRecordIdentity{*tag, car_record_ordinal};
}

std::optional<TireDefinition> parse_tire_resource(
    const std::span<const std::byte> payload)
{
    constexpr std::size_t label_size = 32;
    constexpr std::size_t parameter_count = 22;
    constexpr std::size_t parameter_offset = label_size;
    constexpr std::size_t expected_size = label_size + parameter_count * sizeof(float);
    if (payload.size() != expected_size) {
        return std::nullopt;
    }

    TireDefinition tire;
    std::size_t label_length = 0;
    while (label_length < label_size && payload[label_length] != std::byte{0}) {
        tire.label.push_back(static_cast<char>(
            std::to_integer<unsigned char>(payload[label_length])));
        ++label_length;
    }

    for (std::size_t i = 0; i < parameter_count; ++i) {
        const auto offset = parameter_offset + i * sizeof(std::uint32_t);
        const auto bits = static_cast<std::uint32_t>(std::to_integer<unsigned char>(payload[offset])) |
            (static_cast<std::uint32_t>(std::to_integer<unsigned char>(payload[offset + 1])) << 8U) |
            (static_cast<std::uint32_t>(std::to_integer<unsigned char>(payload[offset + 2])) << 16U) |
            (static_cast<std::uint32_t>(std::to_integer<unsigned char>(payload[offset + 3])) << 24U);
        tire.parameters[i] = std::bit_cast<float>(bits);
    }
    return tire;
}

TireResponseObject initialize_tire_response_object(
    const TireDefinition& tire, const float supplied_value_0x2c) noexcept
{
    const auto& p = tire.parameters;
    TireResponseObject result{};
    const float qx1 = p[13] * 4.45F;
    const float qy1 = p[14] * 255.74712F;
    const float qx2 = p[15] * 4.45F;
    const float qy2 = p[16] * 255.74712F;
    const float qy1_over_x1 = qy1 / qx1;
    const float qy2_over_x2 = qy2 / qx2;
    result.words[0] = (qy2_over_x2 - qy1_over_x1) / (qx2 - qx1);
    result.words[1] = qy1_over_x1 - result.words[0] * qx1;

    const float lx1 = p[9] * 4.45F;
    const float ly1 = p[10];
    const float lx2 = p[11] * 4.45F;
    const float ly2 = p[12];
    result.words[2] = (ly2 - ly1) / (lx2 - lx1);
    result.words[3] = ly1 - result.words[2] * lx1;
    result.words[4] = p[17];
    result.words[5] = p[18];
    result.words[6] = p[19];
    for (std::size_t index = 0; index < 4U; ++index)
        result.words[7U + index] = p[5U + index];
    result.words[11] = supplied_value_0x2c;
    result.words[12] = p[3] * 0.4545F;
    result.words[13] = p[4] * 0.042279862F;
    result.words[14] = static_cast<float>(std::sin(
        static_cast<long double>(p[20]) * 0.017453292F));
    result.words[15] = p[21];
    return result;
}

float evaluate_tire_quadratic(const TireQuadraticTerms& terms,
                              const float input) noexcept
{
    return terms.linear * input + input * input * terms.quadratic;
}

float evaluate_tire_linear(const TireLinearTerms& terms,
                            const float input) noexcept
{
    return terms.slope * input + terms.intercept;
}

TireResponseCurveSet derive_tire_response_curves(
    const TireDefinition& tire) noexcept
{
    const float quadratic_x1 = tire.parameters[13] * 4.45F;
    const float quadratic_y1 = tire.parameters[14] * 255.74712F;
    const float quadratic_x2 = tire.parameters[15] * 4.45F;
    const float quadratic_y2 = tire.parameters[16] * 255.74712F;
    const float quadratic_y1_over_x1 = quadratic_y1 / quadratic_x1;
    const float quadratic_y2_over_x2 = quadratic_y2 / quadratic_x2;
    const float quadratic_coefficient =
        (quadratic_y2_over_x2 - quadratic_y1_over_x1) /
        (quadratic_x2 - quadratic_x1);

    const float linear_x1 = tire.parameters[9] * 4.45F;
    const float linear_y1 = tire.parameters[10];
    const float linear_x2 = tire.parameters[11] * 4.45F;
    const float linear_y2 = tire.parameters[12];
    const float linear_slope = (linear_y2 - linear_y1) / (linear_x2 - linear_x1);

    return {
        {quadratic_coefficient,
         quadratic_y1_over_x1 - quadratic_coefficient * quadratic_x1},
        {linear_slope, linear_y1 - linear_slope * linear_x1},
        tire.parameters[17],
        tire.parameters[18],
        tire.parameters[19],
        static_cast<float>(std::sin(
            static_cast<long double>(tire.parameters[20]) * 0.017453292F)),
        tire.parameters[21]};
}

TireCurveEvaluation evaluate_tire_response_curves(
    const TireResponseCurveSet& curves, const float input) noexcept
{
    const float quadratic = evaluate_tire_quadratic(curves.quadratic, input);
    const float linear = evaluate_tire_linear(curves.linear, input);
    return {
        quadratic,
        quadratic * curves.quadratic_scale_at_0x10,
        quadratic * curves.quadratic_scale_at_0x14,
        linear,
        linear * curves.linear_scale_at_0x18};
}

TireSlipCurveEvaluation evaluate_tire_slip_curve(
    const TireResponseCurveSet& curves,
    const std::uint32_t direction_sign_bits,
    const float input) noexcept
{
    auto signed_input = input;
    if (0x80000000U < direction_sign_bits) {
        signed_input = -signed_input;
    }
    auto clamped_input = signed_input;
    if (clamped_input < -curves.input_limit_at_0x38) {
        clamped_input = -curves.input_limit_at_0x38;
    }
    if (curves.input_limit_at_0x38 < clamped_input) {
        clamped_input = curves.input_limit_at_0x38;
    }
    const auto normalized_output =
        (static_cast<long double>(curves.input_slope_at_0x3c) /
         curves.input_limit_at_0x38) * clamped_input + 1.0L;
    return {signed_input, clamped_input, static_cast<float>(normalized_output)};
}

TireTrigonometricTerms derive_tire_trigonometric_terms(
    const TireDefinition& tire) noexcept
{
    return {tire.parameters[5], tire.parameters[6],
            tire.parameters[7], tire.parameters[8]};
}

float evaluate_tire_trigonometric_response(
    const TireTrigonometricTerms& terms, const float input) noexcept
{
    const auto coefficient_1c = static_cast<long double>(terms.coefficient_0x1c);
    const auto coefficient_28 = static_cast<long double>(terms.coefficient_0x28);
    const auto first_angle = std::atan(coefficient_1c * input);
    const auto second_angle = std::atan(
        (((1.0L - coefficient_28) * input +
          (coefficient_28 / coefficient_1c) * first_angle) * coefficient_1c));
    return static_cast<float>(std::sin(
        second_angle * static_cast<long double>(terms.coefficient_0x20)) *
        static_cast<long double>(terms.coefficient_0x24));
}

WheelTireDynamicsResult advance_wheel_tire_dynamics(
    WheelTireDynamicsState& state,
    const WheelTireDynamicsInput& input) noexcept
{
    WheelTireDynamicsResult result{};
    auto instantaneous_0x2c = input.instantaneous_component_0x2c;
    auto instantaneous_0x30 = input.instantaneous_component_0x30;
    if (input.clear_filter_state) {
        state.filtered_component_0x170 = 0.0F;
        state.filtered_component_0x174 = 0.0F;
        state.suspension_response_0x178 = 0.0F;
        instantaneous_0x2c = 0.0F;
        instantaneous_0x30 = 0.0F;
    }
    state.instantaneous_component_0x180 = instantaneous_0x2c;

    constexpr auto low_gate_bits = std::int32_t{0x41200001};
    const auto low_speed_gate =
        std::bit_cast<std::int32_t>(input.smoothing_gate_value_0xec) < low_gate_bits;
    result.smoothing_weight = low_speed_gate ? 0.6F : 0.4F;
    result.second_blend_weight = low_speed_gate ? 0.2F : 0.0F;

    const auto inverse_smoothing = 1.0F - result.smoothing_weight;
    state.filtered_component_0x174 =
        state.filtered_component_0x174 * result.smoothing_weight +
        inverse_smoothing * instantaneous_0x2c;
    state.filtered_component_0x170 =
        state.filtered_component_0x170 * result.smoothing_weight +
        (1.0F - result.second_blend_weight) * instantaneous_0x30;

    result.combined_slip_magnitude = std::sqrt(
        state.filtered_component_0x174 * state.filtered_component_0x174 +
        input.secondary_slip_component_0xe4 * input.secondary_slip_component_0xe4);
    if (!input.contact_active) {
        state.response_gate_0x11c = 0.0F;
    } else {
        state.response_gate_0x11c =
            result.combined_slip_magnitude * 0.6666667F - 1.0F;
        if (state.response_gate_0x11c <= 0.0F) {
            state.response_gate_0x11c = 0.0F;
        }
    }
    result.trigonometric_response = evaluate_tire_trigonometric_response(
        input.tire_terms, result.combined_slip_magnitude);
    return result;
}

WheelTireAccumulatorResult advance_wheel_tire_accumulators(
    const WheelTireAccumulatorInput& input) noexcept
{
    WheelTireAccumulatorResult result{};
    result.state_0xec = input.state_0xec;
    result.state_0xf0 = input.state_0xf0;
    result.fdiv_compatibility_path = input.fdiv_compatibility_enabled;
    const auto numerator = static_cast<float>(
        static_cast<long double>(input.forward_response_local_0x38) *
            input.forward_accumulator_scale + input.accumulator_0xa4);
    const auto denominator = static_cast<long double>(input.response_gate_0x11c) + 1.0L;
    const auto sign_bits = std::bit_cast<std::uint32_t>(input.sign_source_0x04);
    result.sign_0x04 = sign_bits <= 0x80000000U ? 1.0F : -1.0F;
    const auto averaged = static_cast<float>(
        static_cast<long double>(numerator) / denominator);
    result.accumulator_0xa4 = static_cast<float>(
        static_cast<long double>(averaged) +
        static_cast<long double>(result.sign_0x04) *
            input.lateral_response_local_0x2c * input.signed_lateral_scale);
    result.accumulator_0x10c = static_cast<float>(
        static_cast<long double>(input.accumulator_0x10c) * input.decay_0x10c +
        static_cast<long double>(input.forward_response_local_0x38) *
            input.forward_scale_0x10c);
    if (input.contact_active_0x168) {
        result.state_0xec = static_cast<float>(
            static_cast<long double>(input.forward_response_local_0x38) /
            input.divisor_0x1c);
        const auto divisor = static_cast<long double>(input.divisor_factor_0x48) *
                             input.divisor_factor_0xc0;
        result.state_0xf0 = static_cast<float>(
            static_cast<long double>(input.lateral_response_local_0x2c) / divisor);
        result.contact_states_updated = true;
    }
    return result;
}

WheelRateFinalizationResult finalize_wheel_rate_state(
    const WheelRateFinalizationInput& input) noexcept
{
    const auto rate_delta =
        (1.0L - input.blend_local_0x28) * input.wheel_scalar_0x38 *
        input.response_local_0x90 * input.rate_increment_scale;
    WheelRateFinalizationResult result{};
    result.rate_0x124 = static_cast<float>(
        static_cast<long double>(input.rate_0x124) + rate_delta);
    result.auxiliary_rate_0x128 = static_cast<float>(
        static_cast<long double>(result.rate_0x124) * input.auxiliary_rate_scale);
    result.animation_flags_0x120 = input.animation_flags_0x120;
    if (input.damage_flag_0x20 != 0U) {
        result.animation_flags_0x120 = static_cast<std::uint8_t>(
            result.animation_flags_0x120 | 0x40U);
    }
    return result;
}

WheelSpinPreprojectionResult update_wheel_spin_preprojection(
    const WheelSpinPreprojectionInput& input) noexcept
{
    const auto speed_magnitude = std::abs(input.spin_rate_0x124);
    auto wheel_response = input.damage_flag_0x20 != 0U
        ? input.value_0x50 * 0.2F
        : std::abs(input.value_0xe0) * input.value_0x50;
    const auto wheel_speed = input.use_precomputed_value_0xa8
        ? input.value_0xa8
        : input.spin_rate_0x124 * input.wheel_scalar_0x5c;
    if (input.selector_0x164 != 0 && input.value_0xe0 > 0.0F &&
        input.damage_flag_0x20 == 0U &&
        wheel_speed > 1.0F) {
        const auto threshold = input.selector_0x164 == 1
            ? 1.3333334F : 1.1111112F;
        const auto remaining = threshold - input.state_0x174;
        wheel_response = remaining <= 0.0F
            ? 0.0F : std::min(remaining * 100000.0F, wheel_response);
    }
    auto capped_response = speed_magnitude * wheel_response * 10.1F;
    if (wheel_response <= capped_response) {
        capped_response = wheel_response;
    }
    const auto auxiliary = capped_response * speed_magnitude;
    const auto speed_scale = std::min(speed_magnitude * 0.1F, 1.0F);
    auto response_magnitude = std::abs(input.response_value_0x178);
    if (response_magnitude < 444.99997F) {
        response_magnitude = 444.99997F;
    }
    auto filter = (std::abs(input.value_0x54 * input.spin_rate_0x124 *
                            input.wheel_scalar_0x5c * response_magnitude) +
                   std::abs(speed_scale * input.value_0x58 * response_magnitude) +
                   capped_response + input.previous_filter_0x17c) * 0.5F;
    auto response = input.spin_rate_0x124 > 0.0F ? -filter : filter;
    const auto rate_blend = adjust_wheel_rate_blend({
        input.spin_rate_0x124,
        speed_magnitude,
        std::abs(input.value_0xe0),
        response,
        9.0F,
        -1.0F / 9.0F,
        1.5F,
        -0.2F});
    response = rate_blend.signed_response_local;
    const auto spin_rate = rate_blend.rate_0x124;
    const auto blend = rate_blend.blend;
    auto additional_response = input.wheel_scalar_0x5c != 0.0F
        ? input.numerator_0xe4 / input.wheel_scalar_0x5c
        : 0.0F;
    const auto gated_mode = input.gate_0x160 && input.mode_0x15c == 0;
    if ((gated_mode || input.mode_0x15c != 0) &&
        additional_response > 0.0F &&
        input.spin_rate_0x124 * input.wheel_scalar_0x5c > 1.0F) {
        const auto mode_weight = input.mode_0x15c == 2 ? 5.0F : 0.0F;
        auto attenuation = std::sqrt(
            input.state_0x174 * input.state_0x174 +
            input.state_0x170 * input.state_0x170 * mode_weight) -
            (gated_mode ? 2.0F : 0.3508772F);
        if (gated_mode && input.high_speed_projection < 6.6666665F) {
            auto speed_weight =
                (input.high_speed_projection - 3.1111112F) * 0.28125003F;
            if (speed_weight <= 0.0F) {
                speed_weight = 0.0F;
            }
            attenuation *= speed_weight;
        }
        if (attenuation > 0.0F) {
            additional_response *= 1.0F - attenuation * 0.1F;
            if (additional_response <= 0.0F) {
                additional_response = 0.0F;
            }
        }
    }
    response += additional_response;
    return {spin_rate, auxiliary, filter, response, blend};
}

WheelAnimationTimerResult advance_wheel_animation_timer(
    WheelAnimationTimerState& state,
    const float current_time,
    const float parameter) noexcept
{
    const auto pending = static_cast<float>(
        ((static_cast<long double>(current_time) - state.field_0x194) *
             state.field_0x188 + state.field_0x12c) *
            parameter + state.field_0x198);
    state.field_0x198 = pending;
    const auto next_194 = static_cast<float>(
        static_cast<long double>(state.field_0x190) * state.field_0x198 +
        state.field_0x194);
    state.field_0x198 = 0.0F;
    state.field_0x194 = next_194;
    return {pending, state.field_0x194, state.field_0x198};
}

FourWheelAnimationTimerResult advance_four_wheel_animation_timers(
    std::array<WheelAnimationTimerState, 4>& wheel_states,
    const float current_time,
    const float parameter) noexcept
{
    FourWheelAnimationTimerResult result{};
    for (std::size_t index = 0; index < wheel_states.size(); ++index) {
        result.wheel_results[index] = advance_wheel_animation_timer(
            wheel_states[index], current_time, parameter);
    }
    return result;
}

VehicleAnimationUpdateResult advance_vehicle_animation_channels(
    VehicleAnimationChannelState& channel_0x18,
    VehicleAnimationChannelState& channel_0x5c,
    const float current_time,
    const float parameter) noexcept
{
    const auto advance = [current_time, parameter](VehicleAnimationChannelState& channel) {
        channel.pending = static_cast<float>(
            (static_cast<long double>(current_time) - channel.elapsed) *
                channel.rate * parameter + channel.pending);
        channel.elapsed = static_cast<float>(
            static_cast<long double>(channel.pending) * channel.integration_scale +
            channel.elapsed);
        channel.pending = 0.0F;
        return channel;
    };

    return {advance(channel_0x18), advance(channel_0x5c)};
}

VehicleAnimationFrameResult advance_vehicle_animation_frame(
    std::array<WheelAnimationTimerState, 4>& wheel_states,
    VehicleAnimationChannelState& channel_0x18,
    VehicleAnimationChannelState& channel_0x5c,
    const float current_time,
    const float parameter) noexcept
{
    VehicleAnimationFrameResult result{};
    result.wheels = advance_four_wheel_animation_timers(
        wheel_states, current_time, parameter);
    result.vehicle = advance_vehicle_animation_channels(
        channel_0x18, channel_0x5c, current_time, parameter);
    return result;
}

WheelContactFeatureProfile classify_wheel_contact_feature(
    const WheelContactFeatureProfileInput& input) noexcept
{
    WheelContactFeatureProfile result{};
    result.animation_flags_0x120 = 0x20U;
    result.animation_mode_0x121 = 1U;

    const auto response_gate_bits = std::bit_cast<std::int32_t>(input.response_gate_0x11c);
    if (input.feature_0x19c == 0) {
        result.animation_mode_0x121 = 0U;
        if (response_gate_bits > 0x3F666666) {
            result.animation_flags_0x120 = 0x22U;
        }
        if (response_gate_bits > 0x3F333333) {
            result.animation_flags_0x120 = static_cast<std::uint8_t>(
                result.animation_flags_0x120 | 1U);
        }
        result.animation_flags_0x120 = static_cast<std::uint8_t>(
            result.animation_flags_0x120 & 0xDFU);
    } else {
        if (response_gate_bits > 0x3FC00000) {
            result.animation_flags_0x120 = 0x24U;
        }
        result.animation_mode_0x121 = 5U;
        result.response_scale = 0.9F;
        result.secondary_response = 0.1F;
        switch (input.feature_0x19c) {
        case 10:
        case 11:
        case 13:
        case 15:
            result.response_scale = 0.8F;
            result.secondary_response = 2.5F;
            break;
        case 12:
            break;
        case 20:
        case 23:
            break;
        case 21:
            result.animation_flags_0x120 = 0x24U;
            result.secondary_response = 0.2F;
            break;
        default:
            break;
        }
    }

    result.clear_fields_0xec_0xf0_0x11c =
        input.reset_contact || !input.contact_active;
    if (result.clear_fields_0xec_0xf0_0x11c) {
        result.animation_flags_0x120 = 0U;
        result.animation_mode_0x121 = 0U;
    }
    return result;
}

void apply_wheel_contact_feature_profile(
    WheelContactFeatureState& state,
    const WheelContactFeatureProfile& profile) noexcept
{
    state.animation_flags_0x120 = profile.animation_flags_0x120;
    state.animation_mode_0x121 = profile.animation_mode_0x121;
    if (profile.clear_fields_0xec_0xf0_0x11c) {
        state.state_0xec = 0.0F;
        state.state_0xf0 = 0.0F;
        state.response_gate_0x11c = 0.0F;
    }
}

float advance_wheel_angle_accumulator(
    const WheelAngleAccumulatorInput& input) noexcept
{
    auto angle = static_cast<float>(
        static_cast<long double>(input.rate_0x124) * input.increment_scale +
        input.angle_0xac);
    auto bits = std::bit_cast<std::uint32_t>(angle);
    if (std::bit_cast<std::int32_t>(bits) >
        std::bit_cast<std::int32_t>(input.positive_wrap_threshold)) {
        angle = static_cast<float>(
            static_cast<long double>(angle) - input.wrap_delta);
        bits = std::bit_cast<std::uint32_t>(angle);
    }
    if (bits > std::bit_cast<std::uint32_t>(input.negative_wrap_threshold)) {
        angle = static_cast<float>(
            static_cast<long double>(angle) + input.wrap_delta);
    }
    return angle;
}

WheelUpdatePrefixResult advance_wheel_update_prefix(
    const WheelUpdatePrefixInput& input) noexcept
{
    WheelUpdatePrefixResult result{};
    result.value_0xa8 = static_cast<float>(
        static_cast<long double>(input.rate_0x124) * input.wheel_scalar_0x5c);
    result.damage_flag_0x20 = input.damage_flag_0x20;
    result.distance_gate_ran = input.query_flag_nonzero_0x50878c;
    if (input.query_flag_nonzero_0x50878c) {
        const auto dx = static_cast<float>(
            static_cast<long double>(input.reference_point.x) -
            input.wheel_position_0x14.x);
        const auto dy = static_cast<float>(
            static_cast<long double>(input.reference_point.y) -
            input.wheel_position_0x14.y);
        const auto dz = static_cast<float>(
            static_cast<long double>(input.reference_point.z) -
            input.wheel_position_0x14.z);
        const auto distance_squared =
            static_cast<long double>(dx) * dx +
            static_cast<long double>(dy) * dy +
            static_cast<long double>(dz) * dz;
        if (distance_squared > input.squared_distance_threshold) {
            result.damage_flag_0x20 = true;
            result.damage_flag_set = true;
        }
    }
    result.angle_0xac = advance_wheel_angle_accumulator({
        input.angle_0xac,
        input.rate_0x124,
        input.angle_increment_scale,
        input.positive_angle_threshold,
        input.angle_wrap_delta,
        input.negative_angle_threshold});
    return result;
}

WheelRateBlendResult adjust_wheel_rate_blend(
    const WheelRateBlendInput& input) noexcept
{
    WheelRateBlendResult result{};
    result.signed_response_local = input.signed_response_local;
    result.rate_0x124 = input.rate_0x124;
    const auto magnitude_bits = std::bit_cast<std::int32_t>(
        std::bit_cast<std::uint32_t>(input.rate_magnitude_0x124));
    const auto gate_bits = std::bit_cast<std::int32_t>(
        std::bit_cast<std::uint32_t>(input.rate_gate_0x41100000));
    if (magnitude_bits >= gate_bits) {
        return result;
    }

    result.branch_taken = true;
    auto blend = static_cast<long double>(input.rate_magnitude_0x124) *
                     input.rate_coefficient + 1.0L;
    blend = blend * input.absolute_value_0xe0 * input.blend_coefficient;
    result.blend = static_cast<float>(blend < 1.0L ? blend : 1.0L);
    result.signed_response_local = static_cast<float>(
        (1.0L - result.blend) * input.signed_response_local);
    const auto rate_scale = static_cast<long double>(result.blend) *
                            input.response_rate_coefficient + 1.0L;
    result.rate_0x124 = static_cast<float>(
        static_cast<long double>(input.rate_0x124) * rate_scale);
    return result;
}

WheelExcessWeightResult adjust_wheel_excess_weight(
    const WheelExcessWeightInput& input) noexcept
{
    WheelExcessWeightResult result{input.weight_0x2c, false};
    if (!input.response_path_reached ||
        std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(input.excess_0xc4)) <= 0) {
        return result;
    }

    auto weight = static_cast<long double>(input.excess_0xc4) *
                  input.excess_coefficient + 1.0L;
    weight *= input.weight_0x2c;
    result.weight_0x2c = static_cast<float>(
        weight < input.upper_limit ? weight : input.upper_limit);
    result.applied = true;
    return result;
}

PhysicsVector3 transform_wheel_point_0xc8(
    const WheelPointTransformInput& input) noexcept
{
    const auto point = PhysicsVector3{
        static_cast<float>(static_cast<long double>(input.wheel_position_0x04_0x0c.x) +
                           input.referenced_point_0x10.x),
        static_cast<float>(static_cast<long double>(input.wheel_position_0x04_0x0c.y) +
                           input.referenced_point_0x10.y),
        static_cast<float>(static_cast<long double>(input.wheel_position_0x04_0x0c.z) +
                           input.referenced_point_0x10.z)};
    const auto& m = input.body_transform_0x38_to_0x64;
    const auto transformed_x = static_cast<float>(
        static_cast<long double>(m[1]) * point.x +
        static_cast<long double>(m[4]) * point.y +
        static_cast<long double>(m[7]) * point.z);
    const auto transformed_y = static_cast<float>(
        static_cast<long double>(m[2]) * point.x +
        static_cast<long double>(m[8]) * point.z +
        static_cast<long double>(m[5]) * point.y);
    const auto transformed_z = static_cast<float>(
        static_cast<long double>(m[6]) * point.z +
        static_cast<long double>(m[0]) * point.x +
        static_cast<long double>(m[3]) * point.y);
    return {
        static_cast<float>(static_cast<long double>(m[9]) + transformed_x),
        static_cast<float>(static_cast<long double>(m[10]) + transformed_y),
        static_cast<float>(static_cast<long double>(m[11]) + transformed_z)};
}

WheelAnimationBasisState advance_wheel_animation_basis(
    const WheelAnimationBasisInput& input) noexcept
{
    WheelAnimationBasisState state{};
    const auto side_sign =
        (static_cast<long double>(input.value_0x04) + input.value_0x14) > 0.0L
            ? 1.0L : -1.0L;
    const auto contact_offset = static_cast<float>(
        static_cast<long double>(input.contact_depth_0xe8) -
        static_cast<long double>(input.depth_limit_0x44) * 0.5L);
    long double angle = 0.0L;
    if (!input.damage_flag_0x20) {
        angle = (static_cast<long double>(input.value_0x68) * input.value_0xdc + 1.0L) *
            input.value_0xdc;
        angle += static_cast<long double>(input.value_0x6c) * contact_offset;
        angle += static_cast<long double>(input.value_0x74) * input.scalar_0x10c;
        angle += input.value_0x64;
    } else {
        angle = std::sin(static_cast<long double>(input.steering_angle_0xac)) * 0.08726646L;
        angle += static_cast<long double>(input.value_0x6c) * contact_offset;
        angle += static_cast<long double>(input.value_0x64) * 2.0L;
        angle += static_cast<long double>(input.scalar_0x10c) * 1.9610441e-5L;
        angle += side_sign * 0.2617994L;
    }
    state.angle_0xd4 = static_cast<float>(angle);
    state.sine_0xb0 = static_cast<float>(std::sin(static_cast<long double>(state.angle_0xd4)));
    state.zero_0xb4 = 0.0F;
    state.cosine_0xb8 = static_cast<float>(std::cos(static_cast<long double>(state.angle_0xd4)));

    auto offset = (static_cast<long double>(input.value_0x78) * input.scalar_0x10c +
                   static_cast<long double>(input.value_0x70) * contact_offset +
                   input.value_0x60) * state.cosine_0xb8 -
                  static_cast<long double>(input.value_0x7c) * state.sine_0xb0;
    if (input.damage_flag_0x20) {
        offset += static_cast<long double>(input.scalar_0x10c) * -3.9220882e-5L;
        offset += side_sign * 0.17453292L;
    }
    state.offset_0xd8 = static_cast<float>(offset);

    const auto& body = input.body_basis_0x38_to_0x58;
    const auto& orientation = input.orientation_0x80_0x88;
    state.basis_0x110_0x118[0] = static_cast<float>(
        static_cast<long double>(body[3]) * orientation[1] +
        static_cast<long double>(body[6]) * orientation[2] +
        static_cast<long double>(body[0]) * orientation[0]);
    state.basis_0x110_0x118[1] = static_cast<float>(
        static_cast<long double>(body[7]) * orientation[2] +
        static_cast<long double>(body[1]) * orientation[0] +
        static_cast<long double>(body[4]) * orientation[1]);
    state.basis_0x110_0x118[2] = static_cast<float>(
        static_cast<long double>(body[2]) * orientation[0] +
        static_cast<long double>(body[8]) * orientation[2] +
        static_cast<long double>(body[5]) * orientation[1]);
    return state;
}

float evaluate_wheel_piecewise_response(const WheelPiecewiseTerms& terms,
                                       const float input) noexcept
{
    const auto input_bits = std::bit_cast<std::int32_t>(input);
    const float response = input_bits <= 0
        ? terms.nonpositive_intercept - terms.nonpositive_slope * input
        : terms.positive_slope * input + terms.positive_intercept;
    return response <= 0.0F ? 0.0F : response;
}

WheelContactResult update_wheel_contact(
    WheelContactState& state,
    const WheelContactObservation& observation) noexcept
{
    WheelContactResult result{};
    result.reached_query = observation.body_query_enabled;
    state.contact_feature_0x19c = 0;
    if (observation.body_query_enabled) {
        state.contact_feature_0x19c = observation.query_feature_output_0x19c;
    }

    const auto normal_dot =
        observation.contact_direction.x * observation.query_contact_normal.x +
        observation.contact_direction.y * observation.query_contact_normal.y +
        observation.contact_direction.z * observation.query_contact_normal.z;
    result.direction_alignment = normal_dot;
    if (!observation.body_query_enabled || !observation.query_succeeded ||
        !(normal_dot > 0.1F) || state.contact_feature_0x19c == 0xe) {
        state.contact_active_0x168 = 0U;
        return result;
    }

    result.contact_gate_passed = true;
    if (observation.reset_feature_hook_active) {
        state.contact_feature_0x19c = 0;
        result.reset_feature_id = true;
    }
    result.reached_contact_callback_0x443f00 = true;
    state.contact_point_0x130 = observation.query_contact_point;

    const auto depth =
        (observation.query_contact_point.x - observation.contact_projection_origin.x) *
            observation.contact_direction.x +
        (observation.query_contact_point.y - observation.contact_projection_origin.y) *
            observation.contact_direction.y +
        (observation.query_contact_point.z - observation.contact_projection_origin.z) *
            observation.contact_direction.z;
    result.projected_depth = depth;
    state.contact_active_0x168 = 0U;
    if (depth >= 0.0F) {
        auto contact_scale = std::abs(normal_dot) * 2.0F - 0.5F;
        if (contact_scale <= 0.0F) {
            contact_scale = 0.0F;
        }
        if (1.0F <= contact_scale) {
            contact_scale = 1.0F;
        }
        auto contact_depth = contact_scale * depth;
        const auto maximum_depth = observation.wheel_depth_limit_0x44 + 3.0F;
        if (maximum_depth <= contact_depth) {
            contact_depth = maximum_depth;
        }
        result.clamped_contact_depth = contact_depth;
        state.contact_active_0x168 = 1U;
        result.active = true;
    }
    return result;
}

WheelContactResult update_wheel_contact_from_terrain(
    WheelContactState& state,
    const WheelContactObservation& observation,
    const TerrainSurfaceQueryInput& terrain) noexcept
{
    auto resolved = observation;
    resolved.query_succeeded = false;
    if (observation.body_query_enabled) {
        const auto sample = sample_terrain_surface(terrain);
        resolved.query_succeeded = sample.query_succeeded;
        if (sample.query_succeeded) {
            resolved.query_contact_point = {
                sample.sampled_position[0],
                sample.sampled_position[1],
                sample.sampled_position[2],
            };
            resolved.query_contact_normal = {
                sample.plane_normal[0],
                sample.plane_normal[1],
                sample.plane_normal[2],
            };
            resolved.query_feature_output_0x19c =
                static_cast<std::int32_t>(sample.surface_value_0x34);
        }
    }
    return update_wheel_contact(state, resolved);
}

WheelContactResult update_wheel_contact_from_transformed_point(
    WheelContactState& state,
    const WheelContactObservation& observation,
    const WheelPointTransformInput& point_input,
    const TerrainSurfaceQueryInput& terrain) noexcept
{
    const auto point = transform_wheel_point_0xc8(point_input);
    auto resolved = observation;
    resolved.contact_projection_origin = point;
    auto query = terrain;
    query.position = {point.x, point.y, point.z};
    return update_wheel_contact_from_terrain(state, resolved, query);
}

WheelSuspensionResult evaluate_wheel_suspension(
    const WheelSuspensionInput& input) noexcept
{
    WheelSuspensionResult result{};
    result.scaled_travel = (input.contact_depth - input.previous_contact_depth_0xe8) * 62.5F;
    result.threshold = input.wheel_depth_limit_0x44 - 0.0254F;
    if (result.threshold < input.contact_depth) {
        result.threshold_adjustment =
            (input.contact_depth - result.threshold) * input.suspension_threshold_0x4c;
    }
    result.piecewise_response = evaluate_wheel_piecewise_response(
        input.piecewise_terms, result.scaled_travel);
    result.spring_adjustment = input.spring_scale_0x3c * input.contact_depth;
    result.combined_response_0x178 = result.piecewise_response * result.scaled_travel +
        input.spring_base_0x40 + input.suspension_base_0x158 +
        result.spring_adjustment + result.threshold_adjustment;
    if (input.subtract_damage_adjustment_0x20) {
        result.combined_response_0x178 -= result.spring_adjustment * 0.9F;
    }
    return result;
}

NcarSurfacePolaritySeedPair make_ncar_surface_polarity_seed_pair_0x4193c0(
    const std::uint32_t first_timer_low_dword,
    const std::uint32_t second_timer_low_dword) noexcept
{
    constexpr std::int32_t divisor = 30000;
    const auto first_signed = std::bit_cast<std::int32_t>(first_timer_low_dword);
    const auto second_signed = std::bit_cast<std::int32_t>(second_timer_low_dword);
    return {
        second_signed % divisor,
        first_signed % divisor};
}

std::optional<float> advance_ncar_surface_polarity_phase_0x419620(
    NcarSurfacePolarityState& state) noexcept
{
    if (!state.initialized || state.index_a_0x4f0aa4 >= state.table_0x4f0910.size() ||
        state.index_b_0x4f0aa8 >= state.table_0x4f0910.size() ||
        !std::isfinite(state.phase_0x4f0908) ||
        !std::isfinite(state.increment_0x4f0a94) ||
        !std::isfinite(state.wrap_0x4f0a98)) {
        return std::nullopt;
    }

    auto value = state.table_0x4f0910[state.index_a_0x4f0aa4] -
        state.table_0x4f0910[state.index_b_0x4f0aa8];
    if (!std::isfinite(value)) return std::nullopt;
    if (value < 0.0F) value += 1.0F;
    auto next_phase = state.phase_0x4f0908 - state.increment_0x4f0a94;
    if (next_phase < 0.0F) {
        next_phase += state.wrap_0x4f0a98;
    }
    auto result = value - next_phase;
    if (result < 0.0F) result += 1.0F;
    if (!std::isfinite(next_phase) || !std::isfinite(result)) return std::nullopt;

    state.table_0x4f0910[state.index_a_0x4f0aa4] = value;
    state.index_a_0x4f0aa4 = state.index_a_0x4f0aa4 == 0U
        ? 0x60U : state.index_a_0x4f0aa4 - 1U;
    state.index_b_0x4f0aa8 = state.index_b_0x4f0aa8 == 0U
        ? 0x60U : state.index_b_0x4f0aa8 - 1U;
    state.phase_0x4f0908 = next_phase;
    return result;
}

std::optional<std::int64_t> advance_ncar_surface_polarity_value_0x4193f0(
    NcarSurfacePolarityState& state,
    const std::int32_t scale) noexcept
{
    const auto phase = advance_ncar_surface_polarity_phase_0x419620(state);
    if (!phase.has_value()) return std::nullopt;
    const auto scaled = std::nearbyint(
        static_cast<long double>(*phase) * static_cast<long double>(scale));
    if (!std::isfinite(scaled) ||
        scaled < static_cast<long double>(std::numeric_limits<std::int64_t>::min()) ||
        scaled >= static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(scaled);
}

WheelSuspensionSurfaceResult adjust_wheel_suspension_for_surface(
    const WheelSuspensionSurfaceInput& input) noexcept
{
    WheelSuspensionSurfaceResult result{};
    result.absolute_motion_projection = std::abs(input.signed_motion_projection);
    const auto capped_projection = result.absolute_motion_projection < 1.0F
        ? result.absolute_motion_projection : 1.0F;
    result.motion_weight = capped_projection *
        (input.coefficient_0xa0 * result.absolute_motion_projection +
         input.signed_motion_projection * input.signed_motion_projection * input.coefficient_0x9c);
    auto adjustment = input.environment_sign * result.motion_weight * input.suspension_response;
    if (!input.special_surface_scaling_disabled_0x1a0 &&
        input.contact_feature_0x19c == 10) {
        adjustment *= 9.0F;
    } else if (input.contact_feature_0x19c == 0) {
        adjustment = 0.0F;
    } else {
        adjustment *= 3.0F;
    }
    result.signed_surface_adjustment = input.environment_sign * result.motion_weight *
        input.suspension_response;
    result.feature_scaled_adjustment = adjustment;
    result.adjusted_response = input.suspension_response + adjustment * 0.3F;
    return result;
}

WheelSuspensionFinalizeResult finalize_wheel_suspension(
    const WheelSuspensionFinalizeInput& input) noexcept
{
    WheelSuspensionFinalizeResult result{};
    result.applied_response = input.adjusted_response;
    result.next_contact_depth_0xe8 = input.contact_depth;
    const auto response_bits = std::bit_cast<std::uint32_t>(input.adjusted_response);
    if (0x80000000U < response_bits || !input.contact_active) {
        result.applied_response = 0.0F;
        result.reset_contact = true;
        const auto scaled_spring = input.piecewise_response * 62.499996F;
        result.next_contact_depth_0xe8 =
            (input.previous_contact_depth_0xe8 * scaled_spring - input.suspension_base_0x158) /
            (scaled_spring + input.spring_scale_0x3c);
    }
    if (std::bit_cast<std::int32_t>(result.applied_response) >
        std::bit_cast<std::int32_t>(17800.0F)) {
        result.applied_response = 17800.0F;
    }
    result.applied_force = {
        input.contact_direction.x * result.applied_response,
        input.contact_direction.y * result.applied_response,
        input.contact_direction.z * result.applied_response};
    return result;
}

WheelSuspensionPipelineResult evaluate_wheel_suspension_pipeline(
    const WheelSuspensionPipelineInput& input) noexcept
{
    WheelSuspensionPipelineResult result{};
    result.base = evaluate_wheel_suspension(input.base);

    auto surface_input = input.surface;
    surface_input.suspension_response = result.base.combined_response_0x178;
    result.surface = adjust_wheel_suspension_for_surface(surface_input);

    const WheelSuspensionFinalizeInput finalize_input{
        result.surface.adjusted_response,
        input.base.contact_depth,
        input.base.previous_contact_depth_0xe8,
        result.base.piecewise_response,
        input.base.suspension_base_0x158,
        input.base.spring_scale_0x3c,
        input.contact_direction,
        input.contact_active};
    result.finalized = finalize_wheel_suspension(finalize_input);
    return result;
}

WheelUpdatePipelineResult advance_wheel_update_pipeline(
    WheelContactState& contact_state,
    const WheelUpdatePipelineInput& input) noexcept
{
    WheelUpdatePipelineResult result{};
    result.contact = update_wheel_contact_from_transformed_point(
        contact_state, input.contact, input.point_transform, input.terrain);

    auto suspension = input.suspension;
    suspension.base.contact_depth = result.contact.clamped_contact_depth;
    suspension.contact_active = result.contact.active;
    suspension.contact_direction = input.contact.contact_direction;
    suspension.surface.contact_feature_0x19c = contact_state.contact_feature_0x19c;
    result.suspension = evaluate_wheel_suspension_pipeline(suspension);

    if (result.suspension.finalized.reset_contact) {
        contact_state.contact_active_0x168 = 0U;
    }
    result.feature_state = input.feature_state;
    result.feature_profile = classify_wheel_contact_feature({
        contact_state.contact_feature_0x19c,
        input.feature_state.response_gate_0x11c,
        result.suspension.finalized.reset_contact,
        contact_state.contact_active_0x168 != 0U});
    apply_wheel_contact_feature_profile(result.feature_state, result.feature_profile);
    return result;
}

FourWheelContactUpdateResult advance_four_wheel_contact_updates(
    const FourWheelContactUpdateInput& input) noexcept
{
    FourWheelContactUpdateResult result{};
    result.contact_states = input.contact_states;
    constexpr std::size_t wheel_stride_bytes = 0x1a4U;
    for (std::size_t wheel = 0; wheel < input.wheel_inputs.size(); ++wheel) {
        result.wheel_subobject_offsets[wheel] = wheel * wheel_stride_bytes;
        result.wheel_results[wheel] = advance_wheel_update_pipeline(
            result.contact_states[wheel], input.wheel_inputs[wheel]);
        ++result.wheel_update_calls;
    }
    return result;
}

WheelCollisionImpulseResult prepare_wheel_collision_impulse(
    const WheelCollisionImpulseInput& input) noexcept
{
    WheelCollisionImpulseResult result{};
    if (std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(
            input.contact_threshold_response_0x28)) <=
            std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(0.01F)) ||
        input.wheel_damage_flag_0x20) {
        return result;
    }
    result.normal_response = solve_normal_contact_impulse(
        input.body_center, input.contact_point, input.body_velocity,
        input.contact_normal, input.body_mass, input.angular_response_matrix,
        0.0F);
    if (!(result.normal_response > 0.01F)) {
        return result;
    }

    result.applied = true;
    const auto response_bits = std::bit_cast<std::uint32_t>(result.normal_response);
    if (response_bits > 0x4534c7ffU) {
        result.damage_hook_reached = true;
        result.set_wheel_damage_flag = input.query_flag_nonzero_0x50878c;
    }
    result.impulse = {
        input.contact_normal.x * result.normal_response,
        input.contact_normal.y * result.normal_response,
        input.contact_normal.z * result.normal_response};
    result.duration = input.contact_depth * 1000.0F;
    return result;
}

WheelKinematicRatios derive_wheel_kinematic_ratios(
    const float lateral_velocity,
    const float longitudinal_velocity,
    const float rolling_surface_speed,
    const float clamped_reference_speed) noexcept
{
    const float denominator = std::abs(clamped_reference_speed);
    return {
        lateral_velocity / denominator,
        -(rolling_surface_speed - longitudinal_velocity) / denominator};
}

TireWheelResponseComponents combine_tire_wheel_response(
    const TireWheelResponseFactors& factors) noexcept
{
    return {
        factors.trigonometric_response * factors.forward_ratio *
            factors.ratio_normalization * factors.low_speed_weight *
            factors.curve_input * factors.forward_curve_value,
        (factors.trigonometric_response * factors.lateral_ratio *
             factors.ratio_normalization * factors.lateral_blend *
             factors.lateral_direction_factor + factors.lateral_correction) *
            factors.low_speed_weight * factors.curve_input *
            factors.lateral_curve_value};
}

PhysicsVector3 derive_body_point_velocity_0x43e9b0(
    const BodyPointVelocityInput& input) noexcept
{
    const NcarPointTransformFields body{
        {input.linear_velocity_0x234.x, input.linear_velocity_0x234.y,
         input.linear_velocity_0x234.z},
        {input.angular_velocity_0x240.x, input.angular_velocity_0x240.y,
         input.angular_velocity_0x240.z},
        {input.body_center_0x5c.x, input.body_center_0x5c.y,
         input.body_center_0x5c.z}};
    const auto velocity = transform_ncar_vector_at_point(
        body, {input.point_0x130.x, input.point_0x130.y, input.point_0x130.z});
    return {velocity[0], velocity[1], velocity[2]};
}

float project_body_point_velocity_0x442530(
    const PhysicsVector3& point_velocity,
    const std::array<float, 12>& body_transform_0x38_to_0x64) noexcept
{
    const auto& transform = body_transform_0x38_to_0x64;
    return transform[6] * point_velocity.x +
        transform[8] * point_velocity.y +
        transform[7] * point_velocity.z;
}

WheelMomentVectorResult derive_wheel_moment_vector_0x43e7f0(
    const WheelMomentVectorInput& input) noexcept
{
    const auto& matrix = input.body_basis_0x38_to_0x58;
    const auto& local = input.wheel_local_vector_0xb0;
    WheelMomentVectorResult result{};
    result.transformed_wheel_axis = {
        matrix[0] * local.x + matrix[3] * local.y + matrix[6] * local.z,
        matrix[1] * local.x + matrix[4] * local.y + matrix[7] * local.z,
        matrix[2] * local.x + matrix[5] * local.y + matrix[8] * local.z};
    const PhysicsVector3 car_axis{matrix[3], matrix[4], matrix[5]};
    const auto& wheel_axis = result.transformed_wheel_axis;
    result.wheel_axis_cross_car_axis = {
        wheel_axis.y * car_axis.z - wheel_axis.z * car_axis.y,
        wheel_axis.z * car_axis.x - wheel_axis.x * car_axis.z,
        wheel_axis.x * car_axis.y - wheel_axis.y * car_axis.x};
    const auto prior_spin_delta = -input.previous_spin_rate_0x13c;
    const auto spin_rate = input.spin_rate_0x124;
    result.next_spin_rate_0x13c = spin_rate;
    const auto scale = input.wheel_scale_0x34 * 12.5F;
    result.moment_vector_0x258 = {
        (input.spin_filter_0x140_to_0x148[0] * prior_spin_delta +
         result.wheel_axis_cross_car_axis.x * spin_rate) * scale,
        (input.spin_filter_0x140_to_0x148[1] * prior_spin_delta +
         result.wheel_axis_cross_car_axis.y * spin_rate) * scale,
        (input.spin_filter_0x140_to_0x148[2] * prior_spin_delta +
         result.wheel_axis_cross_car_axis.z * spin_rate) * scale};
    return result;
}

TireWheelResponseFactors derive_tire_wheel_response_factors(
    TireWheelResponseFactors factors,
    const TireWheelCurveFactorInput& input) noexcept
{
    return derive_tire_wheel_response(factors, input).factors;
}

TireWheelResponseDerivation derive_tire_wheel_response(
    TireWheelResponseFactors factors,
    const TireWheelCurveFactorInput& input) noexcept
{
    TireWheelResponseDerivation result{};
    auto curve_projection =
        (static_cast<long double>(input.suspension_response_components[0]) +
         input.wheel_response_components[0]) * input.contact_normal_components[0] +
        (static_cast<long double>(input.suspension_response_components[1]) +
         input.wheel_response_components[1]) * input.contact_normal_components[1] +
        (static_cast<long double>(input.suspension_response_components[2]) +
         input.wheel_response_components[2]) * input.contact_normal_components[2];
    if (curve_projection <= 1.0L) {
        curve_projection = 1.0L;
    }
    const auto curve_input = static_cast<float>(
        curve_projection / input.wheel_scalar_0x90);
    auto reference_speed = input.longitudinal_velocity;
    auto low_speed_weight = 1.0F;
    const auto reference_speed_scale = input.use_reference_speed_scale_0x1c
        ? input.reference_speed_scale_0x1c : input.mode_scale_0xf0;
    const auto reference_speed_limit = reference_speed_scale * 10.0F;
    if (std::abs(reference_speed) < reference_speed_limit) {
        reference_speed = reference_speed_limit;
        if (std::bit_cast<std::int32_t>(input.longitudinal_velocity) <= 0) {
            reference_speed = -reference_speed_limit;
        }
        low_speed_weight = std::abs(reference_speed) / reference_speed_limit;
    }
    const auto ratios = derive_wheel_kinematic_ratios(
        input.lateral_velocity, input.longitudinal_velocity,
        input.rolling_surface_speed, reference_speed);
    const auto curves = evaluate_tire_response_curves(
        input.curves, curve_input);
    const auto slip_curve = evaluate_tire_slip_curve(
        input.curves, input.slip_direction_sign_bits, input.slip_curve_input);
    factors.ratio_normalization = curves.quadratic * input.tire_scale_0x98;
    factors.forward_ratio = ratios.lateral_velocity_ratio;
    factors.lateral_ratio = ratios.rolling_speed_mismatch_ratio;
    factors.low_speed_weight = low_speed_weight;
    factors.lateral_blend = curves.quadratic_scaled_at_0x14 * input.tire_scale_0x98;
    factors.curve_input = curve_input;
    factors.forward_curve_value = curves.linear * input.wheel_scale_0x94 *
        input.mode_scale_0xf0 * slip_curve.normalized_output;
    factors.lateral_curve_value = curves.linear_scaled_at_0x18 *
        input.wheel_scale_0x94 * input.mode_scale_0xf0;
    const auto dynamics_denominator =
        factors.forward_curve_value * factors.curve_input;
    result.dynamics_division_valid =
        dynamics_denominator != 0.0F && std::isfinite(dynamics_denominator);
    if (result.dynamics_division_valid) {
        auto lateral_direction_adjustment = curves.quadratic_scaled_at_0x10 /
            dynamics_denominator * input.slip_curve_input;
        if (lateral_direction_adjustment > 0.9F) {
            lateral_direction_adjustment = 0.9F;
        }
        if (lateral_direction_adjustment < -0.9F) {
            lateral_direction_adjustment = -0.9F;
        }
        factors.lateral_correction = lateral_direction_adjustment;
        const auto direction =
            std::signbit(curves.quadratic_scaled_at_0x10) !=
                    std::signbit(dynamics_denominator)
                ? -1.0F : 1.0F;
        factors.lateral_direction_factor =
            1.0F - direction * lateral_direction_adjustment;
    }
    if (result.dynamics_division_valid) {
        result.dynamics_component_0x30 =
            factors.ratio_normalization / dynamics_denominator *
            factors.forward_ratio;
    }
    const auto lateral_denominator =
        factors.lateral_curve_value * factors.curve_input;
    if (lateral_denominator != 0.0F && std::isfinite(lateral_denominator)) {
        result.dynamics_component_0x2c = factors.lateral_blend /
            lateral_denominator * factors.lateral_ratio * factors.low_speed_weight;
    }
    result.secondary_slip_component_0xe4 = result.dynamics_division_valid
        ? result.dynamics_component_0x30 / factors.lateral_direction_factor
        : input.slip_curve_input;
    result.factors = factors;
    return result;
}

PhysicsVector3 assemble_tire_response_vector(
    const PhysicsVector3& forward_basis,
    const PhysicsVector3& lateral_basis,
    const float forward_response,
    const float lateral_response,
    const float response_scale) noexcept
{
    return {
        -response_scale * (forward_basis.x * forward_response +
                           lateral_basis.x * lateral_response),
        -response_scale * (forward_basis.y * forward_response +
                           lateral_basis.y * lateral_response),
        -response_scale * (forward_basis.z * forward_response +
                           lateral_basis.z * lateral_response)};
}

PhysicsVector3 correct_tire_response_projection(
    const PhysicsVector3& response_vector,
    const PhysicsVector3& correction_basis) noexcept
{
    const float projection = response_vector.x * correction_basis.x +
        response_vector.y * correction_basis.y +
        response_vector.z * correction_basis.z;
    return {
        response_vector.x - correction_basis.x * projection,
        response_vector.y - correction_basis.y * projection,
        response_vector.z - correction_basis.z * projection};
}

WheelTireResponseResult compose_wheel_tire_response(
    const WheelTireResponseInput& input) noexcept
{
    WheelTireResponseResult result{};
    result.components = combine_tire_wheel_response(input.factors);
    result.lateral_basis = {
        input.contact_normal.y * input.forward_basis.z -
            input.contact_normal.z * input.forward_basis.y,
        input.contact_normal.z * input.forward_basis.x -
            input.contact_normal.x * input.forward_basis.z,
        input.contact_normal.x * input.forward_basis.y -
            input.contact_normal.y * input.forward_basis.x};
    result.uncorrected_vector = derive_wheel_tire_response_vector({
        input.factors, input.forward_basis, input.contact_normal,
        input.response_scale});
    result.corrected_vector = correct_tire_response_projection(
        result.uncorrected_vector, input.projection_basis);
    return result;
}

WheelTireBasisUpdateResult apply_wheel_tire_basis_update(
    const WheelTireBasisUpdateInput& input) noexcept
{
    WheelTireBasisUpdateResult result{};
    result.response_vector_0xf4_0xfc = input.response_vector_after_scaling;
    const auto& vector = input.response_vector_after_scaling;
    const auto& basis = input.basis_0x110_0x118;
    const auto basis_projection =
        static_cast<long double>(basis.x) * vector.x +
        static_cast<long double>(basis.y) * vector.y +
        static_cast<long double>(basis.z) * vector.z;
    result.corrected_vector_0x100_0x108 = {
        static_cast<float>(static_cast<long double>(vector.x) - basis.x * basis_projection),
        static_cast<float>(static_cast<long double>(vector.y) - basis.y * basis_projection),
        static_cast<float>(static_cast<long double>(vector.z) - basis.z * basis_projection)};

    const auto& scalar_basis = input.scalar_projection_basis_0x58_0x60;
    const auto scalar_projection =
        static_cast<long double>(scalar_basis.x) * vector.x +
        static_cast<long double>(scalar_basis.y) * vector.y +
        static_cast<long double>(scalar_basis.z) * vector.z;
    result.response_scalar_0x7c = static_cast<float>(
        static_cast<long double>(input.response_scalar_0x7c) - scalar_projection);
    return result;
}

PhysicsVector3 derive_wheel_tire_response_vector(
    const WheelTireResponseVectorInput& input) noexcept
{
    const auto components = combine_tire_wheel_response(input.factors);
    const PhysicsVector3 lateral_basis{
        input.contact_normal.y * input.forward_basis.z -
            input.contact_normal.z * input.forward_basis.y,
        input.contact_normal.z * input.forward_basis.x -
            input.contact_normal.x * input.forward_basis.z,
        input.contact_normal.x * input.forward_basis.y -
            input.contact_normal.y * input.forward_basis.x};
    PhysicsVector3 vector{
        -input.response_scale * (input.forward_basis.x * components.forward +
            lateral_basis.x * components.lateral),
        -input.response_scale * (input.forward_basis.y * components.forward +
            lateral_basis.y * components.lateral),
        -input.response_scale * (input.forward_basis.z * components.forward +
            lateral_basis.z * components.lateral)};
    const auto magnitude_squared = vector.x * vector.x +
        vector.y * vector.y + vector.z * vector.z;
    if (magnitude_squared > 1.0e8F) {
        const auto inverse_magnitude = 1.0F / std::sqrt(magnitude_squared);
        vector.x *= inverse_magnitude * 10000.0F;
        vector.y *= inverse_magnitude * 10000.0F;
        vector.z *= inverse_magnitude * 10000.0F;
    }
    return vector;
}

PhysicsVector3 transform_body_local_point(
    const BodyPointTransformInput& input) noexcept
{
    const auto& point = input.local_point;
    const auto& basis = input.basis_0x38_to_0x58;
    return {
        static_cast<float>(static_cast<long double>(input.center_0x5c.x) +
            static_cast<long double>(basis[0]) * point.x +
            static_cast<long double>(basis[3]) * point.y +
            static_cast<long double>(basis[6]) * point.z),
        static_cast<float>(static_cast<long double>(input.center_0x5c.y) +
            static_cast<long double>(basis[1]) * point.x +
            static_cast<long double>(basis[4]) * point.y +
            static_cast<long double>(basis[7]) * point.z),
        static_cast<float>(static_cast<long double>(input.center_0x5c.z) +
            static_cast<long double>(basis[2]) * point.x +
            static_cast<long double>(basis[5]) * point.y +
            static_cast<long double>(basis[8]) * point.z),
    };
}

EmbeddedRecordContactResponse prepare_embedded_record_contact(
    const EmbeddedRecordContactInput& input) noexcept
{
    const auto dot = [](const PhysicsVector3& left, const PhysicsVector3& right) {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    };
    const auto scale = [](const PhysicsVector3& value, const float factor) {
        return PhysicsVector3{value.x * factor, value.y * factor, value.z * factor};
    };
    const PhysicsVector3 point_delta{
        input.terrain_point.x - input.record_world_point.x,
        input.terrain_point.y - input.record_world_point.y,
        input.terrain_point.z - input.record_world_point.z};
    EmbeddedRecordContactResponse result{};
    result.penetration = -dot(point_delta, input.terrain_normal);
    result.special_feature_0xe = input.terrain_feature == 0xe;
    result.penetration_increased = input.previous_penetration_0x1c < result.penetration;
    const float feature_scale = result.special_feature_0xe ? 0.04F : 1.0F;
    float spring_scale = input.response_scalar_0x18 * feature_scale * result.penetration;
    spring_scale *= result.penetration_increased ? -0.010000001F : -1.0F;
    result.spring_force = scale(input.terrain_normal, spring_scale);

    if (result.special_feature_0xe) {
        const float speed = std::sqrt(dot(input.point_velocity, input.point_velocity));
        result.velocity_force = scale(input.point_velocity, speed * -80.0F);
    } else {
        const float normal_velocity = -dot(input.terrain_normal, input.point_velocity);
        const PhysicsVector3 tangent_velocity{
            input.point_velocity.x + input.terrain_normal.x * normal_velocity,
            input.point_velocity.y + input.terrain_normal.y * normal_velocity,
            input.point_velocity.z + input.terrain_normal.z * normal_velocity};
        result.velocity_force = scale(tangent_velocity, -3000.0F);
        result.friction_magnitude_before_cap = std::sqrt(
            dot(result.velocity_force, result.velocity_force));
        result.normal_response_limit = dot(input.terrain_normal, result.spring_force) * 1.3299999F;
        if (result.normal_response_limit >= 2.0e7F) {
            result.normal_response_limit = 2.0e7F;
        }
        if (result.normal_response_limit < result.friction_magnitude_before_cap) {
            const float ratio = result.normal_response_limit /
                result.friction_magnitude_before_cap;
            result.velocity_force = scale(result.velocity_force, ratio);
        }
        result.friction_magnitude_after_cap = std::sqrt(
            dot(result.velocity_force, result.velocity_force));
        result.large_force_effect_requested =
            result.friction_magnitude_after_cap > 50000.0F;
    }
    result.force_before_virtual_callback = {
        result.spring_force.x + result.velocity_force.x,
        result.spring_force.y + result.velocity_force.y,
        result.spring_force.z + result.velocity_force.z};
    return result;
}

EmbeddedRecordAuxiliaryEffectPlan plan_embedded_record_auxiliary_effect(
    const PhysicsVector3& first_vector,
    const PhysicsVector3& second_vector,
    const bool effect_slot_0x0c_nonzero) noexcept
{
    EmbeddedRecordAuxiliaryEffectPlan result{};
    result.payload_vector = first_vector;
    result.force_scale = std::sqrt(
        second_vector.x * second_vector.x +
        second_vector.y * second_vector.y +
        second_vector.z * second_vector.z) * 0.0001F;
    if (result.force_scale >= 1.0F) result.force_scale = 1.0F;
    result.optional_effect_callback_requested = effect_slot_0x0c_nonzero;
    return result;
}

EmbeddedRecordAuxiliaryEffectUpdate apply_embedded_record_auxiliary_effect(
    EmbeddedRecordAuxiliaryEffectState& state,
    const PhysicsVector3& first_vector,
    const float force_scale) noexcept
{
    EmbeddedRecordAuxiliaryEffectUpdate result{};
    result.target_present = state.effect_target_present_0x0c;
    if (!result.target_present) return result;

    state.cached_vector_0x1c = first_vector;
    result.cache_written = true;
    if (state.target_scale_0x08 != force_scale) {
        state.target_scale_changed_gate_0x2c = true;
        state.target_scale_0x08 = force_scale;
        result.scale_changed = true;
    }
    state.target_enabled_0x29 = true;
    state.target_flag_0x2b = false;
    state.target_flag_0x2a = false;
    result.target_flags_reset = true;
    return result;
}

EmbeddedRecordEffectEventAppendResult append_embedded_record_effect_event(
    const std::span<std::byte> active_event_buffer,
    const std::size_t buffer_base_offset,
    const std::uint32_t used_bytes,
    const std::int32_t now_tick,
    const std::int32_t last_overflow_warning_tick,
    const EmbeddedRecordAuxiliaryEffectPlan& effect) noexcept
{
    EmbeddedRecordEffectEventAppendResult result{};
    result.used_bytes_after = used_bytes;
    result.last_overflow_warning_tick_after = last_overflow_warning_tick;
    const auto attempted_total = used_bytes + effect.payload_size + 1U;
    if (std::bit_cast<std::int32_t>(attempted_total) > 0x600) {
        result.capacity_exceeded = true;
        const auto warning_cutoff = std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(now_tick) - 3000U);
        if (last_overflow_warning_tick == 0 ||
            last_overflow_warning_tick < warning_cutoff) {
            result.overflow_warning_emitted = true;
            result.last_overflow_warning_tick_after = now_tick;
        }
        return result;
    }

    if (effect.payload_size != 0x14U ||
        buffer_base_offset > active_event_buffer.size() ||
        static_cast<std::size_t>(used_bytes) >
            active_event_buffer.size() - buffer_base_offset) {
        return result;
    }
    const auto event_offset = buffer_base_offset + used_bytes;
    const auto event_size = static_cast<std::size_t>(effect.payload_size) + 1U;
    if (event_size > active_event_buffer.size() - event_offset) {
        return result;
    }
    result.storage_range_valid = true;

    auto write_u32_le = [&active_event_buffer](const std::size_t offset,
                                               const std::uint32_t value) {
        for (std::size_t byte = 0; byte < 4U; ++byte) {
            active_event_buffer[offset + byte] = static_cast<std::byte>(
                static_cast<std::uint8_t>(value >> (byte * 8U)));
        }
    };
    active_event_buffer[event_offset] = static_cast<std::byte>(effect.event_type);
    write_u32_le(event_offset + 1U, effect.payload_identifier);
    write_u32_le(event_offset + 5U, std::bit_cast<std::uint32_t>(effect.payload_vector.x));
    write_u32_le(event_offset + 9U, std::bit_cast<std::uint32_t>(effect.payload_vector.y));
    write_u32_le(event_offset + 13U, std::bit_cast<std::uint32_t>(effect.payload_vector.z));
    write_u32_le(event_offset + 17U, std::bit_cast<std::uint32_t>(effect.force_scale));
    result.enqueued = true;
    result.used_bytes_after = used_bytes + static_cast<std::uint32_t>(event_size);
    return result;
}

std::optional<EmbeddedRecordContactInput> make_embedded_record_contact_input(
    const TerrainSurfaceQueryResult& terrain_query,
    const PhysicsVector3& record_world_point,
    const PhysicsVector3& point_velocity,
    const float response_scalar_0x18,
    const float previous_penetration_0x1c) noexcept
{
    if (!terrain_query.query_succeeded || !terrain_query.plane_was_valid) {
        return std::nullopt;
    }
    return EmbeddedRecordContactInput{
        record_world_point,
        {terrain_query.sampled_position[0],
         terrain_query.sampled_position[1],
         terrain_query.sampled_position[2]},
        {terrain_query.plane_normal[0],
         terrain_query.plane_normal[1],
         terrain_query.plane_normal[2]},
        point_velocity,
        response_scalar_0x18,
        previous_penetration_0x1c,
        terrain_query.surface_value_0x34};
}

PhysicsVector3 finish_embedded_record_update(
    EmbeddedRecordState& state,
    const EmbeddedRecordTailInput& input) noexcept
{
    if (state.copy_body_center_gate_0x2c) {
        state.local_point_0x20 = input.body_center_0x5c;
        state.copy_body_center_gate_0x2c = false;
    }
    const PhysicsVector3 world_point = transform_body_local_point({
        state.local_point_0x20,
        input.body_basis_0x38_to_0x58,
        input.body_center_0x5c});
    state.world_point_cache_0x0c = world_point;
    state.previous_penetration_0x1c = input.terrain_contact_found
        ? input.virtual_response_force_x
        : 0.0F;
    return world_point;
}

EmbeddedRecordForceApplicationResult apply_embedded_record_force_to_body(
    float& energy_accumulator_0xe40,
    NcarForceMomentAccumulator& force_moment_accumulator,
    const PhysicsVector3& body_linear_velocity,
    const PhysicsVector3& response_force,
    const PhysicsVector3& application_point,
    const PhysicsVector3& body_center) noexcept
{
    EmbeddedRecordForceApplicationResult result{};
    result.body_speed_squared =
        body_linear_velocity.x * body_linear_velocity.x +
        body_linear_velocity.z * body_linear_velocity.z +
        body_linear_velocity.y * body_linear_velocity.y;
    if (result.body_speed_squared > 4.0F) {
        result.energy_scale = 1.0F;
        if (std::bit_cast<std::int32_t>(result.body_speed_squared) <
            std::bit_cast<std::int32_t>(10.0F)) {
            result.energy_scale =
                (result.body_speed_squared - 4.0F) * 0.16666667F;
        }
        result.added_energy_0xe40 = result.energy_scale * (
            response_force.x * response_force.x +
            response_force.z * response_force.z +
            response_force.y * response_force.y);
        energy_accumulator_0xe40 += result.added_energy_0xe40;
        result.energy_accumulator_updated = true;
    }

    const std::array<float, 3> force{
        response_force.x, response_force.y, response_force.z};
    const std::array<float, 3> point{
        application_point.x, application_point.y, application_point.z};
    const std::array<float, 3> center{
        body_center.x, body_center.y, body_center.z};
    result.force_moment_accumulator_updated = accumulate_ncar_force_moment(
        force_moment_accumulator, force, point, center);
    return result;
}

EmbeddedRecordUpdateResult update_embedded_record_contact(
    EmbeddedRecordState& record_state,
    float& energy_accumulator_0xe40,
    NcarForceMomentAccumulator& force_moment_accumulator,
    const EmbeddedRecordUpdateInput& input) noexcept
{
    EmbeddedRecordUpdateResult result{};
    const auto& body_center = input.body_motion.center_0x5c;
    if (record_state.copy_body_center_gate_0x2c) {
        record_state.local_point_0x20 = {
            body_center[0], body_center[1], body_center[2]};
        record_state.copy_body_center_gate_0x2c = false;
    }

    result.world_point = transform_body_local_point({
        record_state.local_point_0x20,
        input.body_basis_0x38_to_0x58,
        {body_center[0], body_center[1], body_center[2]}});
    const auto contact_point = input.contact_point_override_enabled
        ? input.contact_point_override : result.world_point;
    float response_force_x = 0.0F;
    if (input.terrain_query.query_succeeded && input.terrain_query.plane_was_valid) {
        const std::array<float, 3> world_point_array{
            contact_point.x, contact_point.y, contact_point.z};
        const auto point_velocity_array = transform_ncar_vector_at_point(
            input.body_motion, world_point_array);
        const PhysicsVector3 point_velocity{
            point_velocity_array[0], point_velocity_array[1], point_velocity_array[2]};
        const auto contact_input = make_embedded_record_contact_input(
            input.terrain_query, contact_point, point_velocity,
            record_state.response_scalar_0x18,
            record_state.previous_penetration_0x1c);
        if (!contact_input.has_value()) {
            result.world_point = finish_embedded_record_update(record_state, {
                {body_center[0], body_center[1], body_center[2]},
                input.body_basis_0x38_to_0x58,
                false,
                0.0F});
            return result;
        }
        result.terrain_contact_found = true;
        result.contact_response = prepare_embedded_record_contact(*contact_input);
        result.body_application = apply_embedded_record_force_to_body(
            energy_accumulator_0xe40, force_moment_accumulator,
            {input.body_motion.linear_0x234[0],
             input.body_motion.linear_0x234[1],
             input.body_motion.linear_0x234[2]},
            result.contact_response.force_before_virtual_callback,
            contact_point,
            {body_center[0], body_center[1], body_center[2]});
        // The observed PCAR and NCAR +0x34 implementations do not modify the
        // force vector, so this is also the callback's resulting X component.
        response_force_x = result.contact_response.force_before_virtual_callback.x;
    }

    result.world_point = finish_embedded_record_update(record_state, {
        {body_center[0], body_center[1], body_center[2]},
        input.body_basis_0x38_to_0x58,
        result.terrain_contact_found,
        response_force_x});
    return result;
}

EmbeddedRecordUpdateResult update_embedded_record_contact_from_terrain(
    EmbeddedRecordState& record_state,
    float& energy_accumulator_0xe40,
    NcarForceMomentAccumulator& force_moment_accumulator,
    const EmbeddedRecordUpdateInput& input,
    const TerrainSurfaceResourcePayload& terrain,
    const PhysicsVector3& record_local_point_0x00,
    const bool compatibility_division_enabled) noexcept
{
    const auto& center = input.body_motion.center_0x5c;
    const PhysicsVector3 start_local = record_state.copy_body_center_gate_0x2c
        ? PhysicsVector3{center[0], center[1], center[2]}
        : record_state.local_point_0x20;
    const PhysicsVector3 end_world = transform_body_local_point({
        record_local_point_0x00, input.body_basis_0x38_to_0x58,
        {center[0], center[1], center[2]}});
    const std::array<float, 3> start{
        start_local.x, start_local.y + 10.0F, start_local.z};
    const std::array<float, 3> end{
        end_world.x, end_world.y, end_world.z};
    const auto segment = query_terrain_resource_segment(
        terrain, start, end, compatibility_division_enabled);

    auto composed_input = input;
    composed_input.contact_point_override_enabled = true;
    composed_input.contact_point_override = end_world;
    composed_input.terrain_query = {};
    composed_input.terrain_query.compatibility_division_path =
        segment.compatibility_division_path;
    if (segment.query_succeeded) {
        composed_input.terrain_query.query_succeeded = true;
        composed_input.terrain_query.partition_tree_was_valid =
            segment.partition_tree_was_valid;
        composed_input.terrain_query.plane_was_valid = segment.plane_was_valid;
        composed_input.terrain_query.selected_plane_index = segment.selected_plane_index;
        composed_input.terrain_query.sampled_position = segment.intersection;
        composed_input.terrain_query.plane_normal = segment.plane_normal;
        composed_input.terrain_query.surface_value_0x34 = segment.surface_value_0x34;
    }
    return update_embedded_record_contact(record_state,
        energy_accumulator_0xe40, force_moment_accumulator, composed_input);
}

EmbeddedRecordBatchUpdateResult update_embedded_record_contact_batch_from_terrain(
    const std::span<EmbeddedRecordState> record_states,
    const std::span<const PhysicsVector3> record_local_points_0x00,
    const std::size_t record_count_0x7b,
    float& energy_accumulator_0xe40,
    NcarForceMomentAccumulator& force_moment_accumulator,
    const EmbeddedRecordUpdateInput& body_input,
    const TerrainSurfaceResourcePayload& terrain,
    const bool compatibility_division_enabled)
{
    EmbeddedRecordBatchUpdateResult result{};
    result.requested_records = record_count_0x7b;
    const auto available_records = std::min(
        record_states.size(), record_local_points_0x00.size());
    result.processed_records = std::min(
        record_count_0x7b, available_records);
    result.unprocessed_records = record_count_0x7b - result.processed_records;
    result.input_complete = result.unprocessed_records == 0U;
    result.record_offsets_0x00.reserve(result.processed_records);
    result.record_results.reserve(result.processed_records);

    constexpr std::size_t record_stride_bytes = 0x30U;
    for (std::size_t index = 0; index < result.processed_records; ++index) {
        result.record_offsets_0x00.push_back(index * record_stride_bytes);
        result.record_results.push_back(
            update_embedded_record_contact_from_terrain(
                record_states[index], energy_accumulator_0xe40,
                force_moment_accumulator, body_input, terrain,
                record_local_points_0x00[index],
                compatibility_division_enabled));
    }
    return result;
}

PhysicsVector3 advance_body_linear_velocity(
    const PhysicsVector3& velocity,
    const PhysicsVector3& accumulated_force,
    const float inverse_mass) noexcept
{
    const float integration_scale = inverse_mass * kBodyIntegrationScale;
    return {
        accumulated_force.x * integration_scale + velocity.x,
        accumulated_force.y * integration_scale + velocity.y,
        accumulated_force.z * integration_scale + velocity.z};
}

BodyLinearStep advance_body_linear_step(
    const PhysicsVector3& position,
    const PhysicsVector3& velocity,
    const PhysicsVector3& accumulated_force,
    const float inverse_mass) noexcept
{
    auto next_velocity = advance_body_linear_velocity(
        velocity, accumulated_force, inverse_mass);
    const float speed_squared = next_velocity.x * next_velocity.x +
        next_velocity.y * next_velocity.y + next_velocity.z * next_velocity.z;
    const bool velocity_capped = speed_squared > 90000.0F;
    if (velocity_capped) {
        const float scale = 300.0F / std::sqrt(speed_squared);
        next_velocity.x *= scale;
        next_velocity.y *= scale;
        next_velocity.z *= scale;
    }

    return {
        next_velocity,
        {position.x + next_velocity.x * kBodyIntegrationScale,
         position.y + next_velocity.y * kBodyIntegrationScale,
         position.z + next_velocity.z * kBodyIntegrationScale},
        velocity_capped};
}

PhysicsVector3 transform_body_angular_accumulator(
    const PhysicsVector3& accumulator,
    const std::array<float, 9>& matrix_0x99) noexcept
{
    return {
        matrix_0x99[0] * accumulator.x + matrix_0x99[3] * accumulator.y +
            matrix_0x99[6] * accumulator.z,
        matrix_0x99[1] * accumulator.x + matrix_0x99[4] * accumulator.y +
            matrix_0x99[7] * accumulator.z,
        matrix_0x99[2] * accumulator.x + matrix_0x99[5] * accumulator.y +
            matrix_0x99[8] * accumulator.z};
}

BodyAngularVelocityStep advance_body_angular_velocity(
    const PhysicsVector3& angular_velocity,
    const PhysicsVector3& transformed_accumulator) noexcept
{
    PhysicsVector3 next{
        angular_velocity.x + transformed_accumulator.x * kBodyIntegrationScale,
        angular_velocity.y + transformed_accumulator.y * kBodyIntegrationScale,
        angular_velocity.z + transformed_accumulator.z * kBodyIntegrationScale};
    const float speed_squared = next.x * next.x + next.y * next.y +
        next.z * next.z;
    const bool capped = speed_squared > 100.0F;
    if (capped) {
        const float scale = 10.0F / std::sqrt(speed_squared);
        next.x *= scale;
        next.y *= scale;
        next.z *= scale;
    }
    return {next, capped};
}

BodyOrientationStep advance_body_orientation_matrix(
    const std::array<float, 9>& orientation_matrix,
    const PhysicsVector3& angular_velocity) noexcept
{
    const PhysicsVector3 rotation_vector{
        angular_velocity.x * kBodyIntegrationScale,
        angular_velocity.y * kBodyIntegrationScale,
        angular_velocity.z * kBodyIntegrationScale};
    const float angle = std::sqrt(
        rotation_vector.x * rotation_vector.x +
        rotation_vector.y * rotation_vector.y +
        rotation_vector.z * rotation_vector.z);

    std::array<float, 9> delta{};
    const bool identity_rotation = angle <= 0.0001F;
    if (identity_rotation) {
        delta = {1.0F, 0.0F, 0.0F,
                 0.0F, 1.0F, 0.0F,
                 0.0F, 0.0F, 1.0F};
    } else {
        const float x = rotation_vector.x / angle;
        const float y = rotation_vector.y / angle;
        const float z = rotation_vector.z / angle;
        const float sine = std::sin(angle);
        const float cosine = std::cos(angle);
        const float one_minus_cosine = 1.0F - cosine;

        // The stack matrix built by 0x43ECE0 is the transposed Rodrigues
        // form shown by its assignments before the row-major multiply.
        delta = {
            x * x * one_minus_cosine + cosine,
            x * y * one_minus_cosine + z * sine,
            x * z * one_minus_cosine - y * sine,
            x * y * one_minus_cosine - z * sine,
            y * y * one_minus_cosine + cosine,
            y * z * one_minus_cosine + x * sine,
            x * z * one_minus_cosine + y * sine,
            y * z * one_minus_cosine - x * sine,
            z * z * one_minus_cosine + cosine};
    }

    std::array<float, 9> updated{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t inner = 0; inner < 3; ++inner) {
                updated[row * 3 + column] +=
                    orientation_matrix[row * 3 + inner] *
                    delta[inner * 3 + column];
            }
        }
    }
    return {updated, delta, angle, identity_rotation};
}

BodyBasisSynchronization synchronize_body_basis_matrices(
    const std::array<float, 9>& local_matrix,
    const std::array<float, 9>& matrix_0x210,
    const std::array<float, 9>& orientation_matrix) noexcept
{
    const auto multiply = [](const std::array<float, 9>& left,
                             const std::array<float, 9>& right) {
        std::array<float, 9> product{};
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t column = 0; column < 3; ++column) {
                for (std::size_t inner = 0; inner < 3; ++inner) {
                    product[row * 3 + column] +=
                        left[row * 3 + inner] * right[inner * 3 + column];
                }
            }
        }
        return product;
    };

    auto intermediate = multiply(local_matrix, matrix_0x210);
    auto basis = multiply(intermediate, orientation_matrix);
    return {local_matrix, intermediate, basis};
}

std::array<float, 9> build_body_integrator_local_basis_matrix(
    const std::array<float, 9>& updated_orientation_matrix) noexcept
{
    // 0x43F2F9 copies nine DWORDs into the temporary matrix. The following
    // stack-relative moves reorder them before the 0x402DD0 multiply. Preserve
    // each loaded source value before applying the overlapping stores.
    auto local = updated_orientation_matrix;
    const auto third = local[3];
    const auto saved_third = local[2];
    local[1] = third;
    const auto sixth = local[5];
    const auto seventh = local[6];
    local[3] = seventh;
    local[2] = seventh;
    local[6] = saved_third;
    const auto eighth = local[7];
    local[5] = eighth;
    local[7] = sixth;
    return local;
}

void finish_body_integration_tail(BodyIntegrationTailState& state) noexcept
{
    state.running_accumulators_0x93_to_0x98.fill(0.0F);
    ++state.update_counter_0x10;
}

bool consume_body_point_impulse_accumulator(
    BodyIntegrationTailState& tail_state,
    NcarPointImpulseAccumulator& impulse_state) noexcept
{
    constexpr float duration_epsilon = 1.1920929e-07F;
    constexpr float integration_scale = 62.5F;
    const bool consumed = std::fabs(impulse_state.duration_0x2ac) > duration_epsilon;
    if (consumed) {
        const float inverse_duration = 1.0F / impulse_state.duration_0x2ac;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const float linear = impulse_state.impulse_time_0x294[axis] * inverse_duration;
            const float angular = impulse_state.moment_time_0x2a0[axis] * inverse_duration;
            tail_state.running_accumulators_0x93_to_0x98[axis] +=
                linear * integration_scale;
            tail_state.running_accumulators_0x93_to_0x98[axis + 3] +=
                angular * integration_scale;
        }
        impulse_state.impulse_time_0x294.fill(0.0F);
        impulse_state.moment_time_0x2a0.fill(0.0F);
    }
    impulse_state.duration_0x2ac = 0.0F;
    return consumed;
}

BodyIntegrationVirtualRecordCalls materialize_body_integration_virtual_record_calls(
    const float duration,
    const std::span<const float> packed_records,
    const std::size_t record_count)
{
    BodyIntegrationVirtualRecordCalls result{};
    constexpr float duration_epsilon = 1.1920929e-07F;
    if (!(std::fabs(duration) > duration_epsilon) || record_count == 0) {
        return result;
    }

    constexpr std::size_t floats_per_record = 7;
    const auto available_records = packed_records.size() / floats_per_record;
    const auto materialized_records = std::min(record_count, available_records);
    result.calls.reserve(materialized_records);
    const float inverse_duration = 1.0F / duration;
    for (std::size_t record = 0; record < materialized_records; ++record) {
        const auto offset = record * floats_per_record;
        result.calls.push_back({
            {packed_records[offset] * inverse_duration,
             packed_records[offset + 1] * inverse_duration,
             packed_records[offset + 2] * inverse_duration},
            {packed_records[offset + 3],
             packed_records[offset + 4],
             packed_records[offset + 5]},
            packed_records[offset + 6]});
    }
    result.truncated_records = record_count - materialized_records;
    return result;
}

BodyIntegrationForceCollectionResult prepare_body_integration_force_collection(
    BodyIntegrationTailState& tail_state,
    NcarPointImpulseAccumulator& impulse_state,
    const BodyIntegrationForceCollectionInput& input)
{
    BodyIntegrationForceCollectionResult result{};
    tail_state.running_accumulators_0x93_to_0x98[1] +=
        input.gravity_mass_0x7c * -9.81F;
    for (std::size_t index = 0;
         index < tail_state.running_accumulators_0x93_to_0x98.size(); ++index) {
        tail_state.running_accumulators_0x93_to_0x98[index] +=
            input.embedded_record_accumulator_delta[index];
    }

    result.virtual_record_calls = materialize_body_integration_virtual_record_calls(
        impulse_state.duration_0x2ac,
        input.packed_virtual_records,
        tail_state.pending_virtual_record_count_0x11c);
    result.point_impulse_consumed = consume_body_point_impulse_accumulator(
        tail_state, impulse_state);
    tail_state.pending_virtual_record_count_0x11c = 0;

    const auto& accumulated = tail_state.running_accumulators_0x93_to_0x98;
    result.accumulated_linear_force = {accumulated[0], accumulated[1], accumulated[2]};
    result.accumulated_angular_moment = {accumulated[3], accumulated[4], accumulated[5]};
    return result;
}

BodyIntegrationKinematicsResult advance_body_integration_kinematics(
    BodyIntegrationTailState& tail_state,
    const BodyIntegrationKinematicsInput& input) noexcept
{
    BodyIntegrationKinematicsResult result{};
    result.linear = advance_body_linear_step(
        input.position, input.linear_velocity,
        input.accumulated_linear_force, input.inverse_mass);
    const auto transformed_angular = transform_body_angular_accumulator(
        input.accumulated_angular_moment,
        input.angular_transform_matrix_0x99);
    result.angular = advance_body_angular_velocity(
        input.angular_velocity, transformed_angular);
    result.orientation = advance_body_orientation_matrix(
        input.orientation_matrix_0x0e, result.angular.angular_velocity);
    const auto local_basis = build_body_integrator_local_basis_matrix(
        result.orientation.orientation_matrix);
    result.basis = synchronize_body_basis_matrices(
        local_basis, input.body_matrix_0x210,
        result.orientation.orientation_matrix);
    finish_body_integration_tail(tail_state);
    return result;
}

float solve_normal_contact_impulse(
    const PhysicsVector3& body_center,
    const PhysicsVector3& contact_point,
    const PhysicsVector3& body_velocity,
    const PhysicsVector3& contact_normal,
    const float body_mass,
    const std::array<float, 9>& angular_response_matrix,
    const float restitution) noexcept
{
    const auto dot = [](const PhysicsVector3& left, const PhysicsVector3& right) {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    };
    const auto cross = [](const PhysicsVector3& left, const PhysicsVector3& right) {
        return PhysicsVector3{
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
    };

    const float normal_velocity = dot(contact_normal, body_velocity);
    if (!(normal_velocity < 0.0F)) {
        return 0.0F;
    }

    const float normal_length = std::sqrt(dot(contact_normal, contact_normal));
    const PhysicsVector3 normal{
        contact_normal.x / normal_length,
        contact_normal.y / normal_length,
        contact_normal.z / normal_length};
    const PhysicsVector3 arm{
        contact_point.x - body_center.x,
        contact_point.y - body_center.y,
        contact_point.z - body_center.z};
    const auto angular_arm = cross(normal, arm);
    const PhysicsVector3 angular_response{
        angular_response_matrix[0] * angular_arm.x +
            angular_response_matrix[3] * angular_arm.y +
            angular_response_matrix[6] * angular_arm.z,
        angular_response_matrix[1] * angular_arm.x +
            angular_response_matrix[4] * angular_arm.y +
            angular_response_matrix[7] * angular_arm.z,
        angular_response_matrix[2] * angular_arm.x +
            angular_response_matrix[5] * angular_arm.y +
            angular_response_matrix[8] * angular_arm.z};

    const float denominator = 1.0F / body_mass +
        dot(normal, cross(arm, angular_response));
    if (!(std::abs(denominator) > 1.0e-7F)) {
        return 0.0F;
    }

    const float impulse = -(1.0F + restitution) *
        dot(normal, body_velocity) / denominator;
    return impulse >= 100000.0F ? 100000.0F : impulse;
}

float inches_to_meters(const float value) noexcept
{
    return value * 0.0254F;
}

float degrees_to_radians(const float value) noexcept
{
    return value * 0.017453292F;
}

bool valid_wheel_lock_bits(const std::uint32_t value_bits) noexcept
{
    return value_bits >= kWheelLockMinimumBits &&
           value_bits <= kWheelLockMaximumBits;
}

bool valid_wheel_lock(const float value) noexcept
{
    return valid_wheel_lock_bits(std::bit_cast<std::uint32_t>(value));
}

} // namespace viper
