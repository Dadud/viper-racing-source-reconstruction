#include "viper/pcar_control_update.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace viper {
namespace {

float clamp_finite(const float value, const float low, const float high) noexcept
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

} // namespace

void update_pcar_control(PcarControlFields& fields, const float requested_value) noexcept
{
    const float delta = clamp_finite(requested_value - fields.value_0x500, -0.24F, 0.24F);
    fields.value_0x500 = clamp_finite(fields.value_0x500 + delta, -1.0F, 1.0F);
    const float scaled = fields.multiplier_0xdcc * fields.value_0x500;

    if (fields.mode_0xbe4) {
        fields.output_0x978 = -scaled;
        fields.output_0xb1c = -scaled;
    } else {
        fields.output_0x630 = scaled;
        fields.output_0x7d4 = scaled;
    }
}

void set_pcar_scalar_0xde0(PcarScalarFields& fields, const float value) noexcept
{
    fields.value_0xde0 = value;
}

void set_pcar_scalar_0xde4(PcarScalarFields& fields, const float value) noexcept
{
    fields.value_0xde4 = value;
}

void set_pcar_scalar_0xde8(PcarScalarFields& fields, const float value) noexcept
{
    fields.value_0xde8 = value;
}

void set_pcar_scalar_0xdec(PcarScalarFields& fields, const float value) noexcept
{
    fields.value_0xdec = value;
}

PcarConstructorControls initialize_pcar_constructor_controls(
    const std::optional<bool> traction_control,
    const std::optional<bool> abs_braking,
    const std::optional<bool> auto_shifting,
    const std::optional<bool> auto_clutch,
    const std::optional<std::int32_t> yaw_control,
    const std::optional<std::uint32_t> game_mode_0x4f4,
    const std::optional<std::uint8_t> value_0xd74) noexcept
{
    PcarConstructorControls result{};
    result.traction_control = traction_control.value_or(true);
    result.abs_braking = abs_braking.value_or(true);
    result.auto_shifting = auto_shifting.value_or(true);
    result.auto_clutch = auto_clutch.value_or(true);
    result.yaw_control = yaw_control.value_or(0);
    result.game_mode_0x4f4 = game_mode_0x4f4;
    result.value_0xd74 = value_0xd74;
    if (result.auto_shifting) {
        result.auto_clutch_forced_by_auto_shift = !result.auto_clutch;
        result.auto_clutch = true;
    }
    result.auto_clutch_0x534 = result.auto_clutch;
    result.wheel_traction_0x6b0.fill(
        static_cast<std::uint32_t>(result.traction_control));
    result.wheel_abs_0x6b8.fill(
        static_cast<std::uint32_t>(result.abs_braking));
    return result;
}

PcarCallbackTailResult advance_pcar_callback_tail(
    PcarCallbackTailFields& fields,
    const float value_from_0x43ba10) noexcept
{
    PcarCallbackTailResult result{};
    constexpr std::int32_t helper_threshold_bits = 0x3c23d70a;
    if (fields.gate_0xc3d != 0U &&
        std::bit_cast<std::int32_t>(fields.value_0xc7c) < helper_threshold_bits) {
        result.called_curve_helper_0x4402d0 = true;
        if (fields.curve_gate_0x41 != 0U) {
            fields.value_0xc3c = 1U;
            result.set_byte_0xc3c = true;
        }
    }

    if (fields.mode_0x4fc == 3U) {
        fields.value_0xde0 = 0.0F;
        result.reset_0xde0 = true;
        constexpr float minimum_0xde4 =
            std::bit_cast<float>(std::uint32_t{0x3e4ccccdU});
        if (fields.value_0xde4 < minimum_0xde4) {
            fields.value_0xde4 = minimum_0xde4;
            result.floored_0xde4 = true;
        }
    }

    if (fields.mode_0xbe4) {
        fields.value_0x508 = value_from_0x43ba10;
        result.called_value_helper_0x434b20 = true;
    }
    return result;
}

void set_pcar_callback_globals_0x43b880(PcarCallbackGlobals& globals,
                                         const std::uint32_t value_0x509384,
                                         const std::uint32_t value_0x509388,
                                         const std::uint32_t value_0x5093a4) noexcept
{
    globals.value_0x509384 = value_0x509384;
    globals.value_0x509388 = value_0x509388;
    globals.value_0x5093a4 = value_0x5093a4;
}

PcarCallbackMultiplierResult evaluate_pcar_callback_multiplier(
    const PcarCallbackMultiplierObservation& observation) noexcept
{
    if (!observation.override_gate_0x4d10fc) {
        return {false, false, 1.0F};
    }
    const bool selector_is_one = observation.selector_0x4e322c == 1U;
    return {true, selector_is_one, selector_is_one ? 1.0F : 0.0F};
}

void update_pcar_callback_scalar_prefix(
    PcarScalarFields& fields,
    const PcarCallbackGlobals& globals,
    const float value_from_0x40d270) noexcept
{
    set_pcar_scalar_0xde0(fields,
        globals.value_0x5093f4 * value_from_0x40d270);
    set_pcar_scalar_0xde4(fields, globals.value_0x509320);
    set_pcar_scalar_0xde8(fields, globals.value_0x5093b8);
    set_pcar_scalar_0xdec(fields,
        pcar_callback_value_0x43b950(globals.value_0x5093c8));
}

PcarCallbackControlResult advance_pcar_callback_control_prefix(
    PcarCallbackControlState& state,
    const PcarCallbackControlObservation& observation) noexcept
{
    const auto& velocity = observation.body_velocity_0x234;
    const auto& basis = observation.body_basis_0x50;
    const float projection = std::abs(
        velocity[0] * basis[0] + velocity[1] * basis[1] +
        velocity[2] * basis[2]);
    const float request = pcar_global_scale_0x43b8c0(
        projection, observation.globals.value_0x509358,
        observation.globals.gate_0x5093fc);
    update_pcar_control(state.smoother, request);
    const auto multiplier =
        evaluate_pcar_callback_multiplier(observation.multiplier);
    update_pcar_callback_scalar_prefix(state.scalar_prefix,
        observation.globals, multiplier.value);
    return {projection, request, multiplier};
}

PcarCallbackBranchResult advance_pcar_callback_branch(
    PcarCallbackBranchFields& fields,
    const PcarCallbackBranchObservation& observation) noexcept
{
    PcarCallbackBranchResult result{};
    const auto& globals = observation.globals;
    const bool call_433210 = !globals.gate_0x5093f0 && fields.value_0xec8 != 0U;
    const bool call_43b9a0 = !globals.gate_0x5093f0 && fields.value_0xec8 == 0U;
    const bool call_4331e0_after_value = call_43b9a0 &&
        (fields.value_0xec9 != 0U ||
         std::bit_cast<std::int32_t>(fields.value_0xc7c) < 0x3f666666);
    if (call_433210 && (!observation.selector_0xc98 ||
                        !observation.curve_0xbfc)) {
        return result;
    }

    if (globals.gate_0x5093f0) {
        constexpr std::int32_t argument = -1;
        fields.value_0xcbc = std::bit_cast<std::uint32_t>(argument);
        fields.value_0xd64 = fields.value_0xcbc;
        result.called_0x4331e0 = true;
        result.argument_0x4331e0 = argument;
        constexpr float constant_0x4c814c =
            std::bit_cast<float>(std::uint32_t{0x3e99999aU});
        fields.value_0x54 = globals.value_0x5093f4;
        if (fields.value_0x54 < constant_0x4c814c) {
            fields.value_0x54 = constant_0x4c814c;
        }
    } else if (call_433210) {
        auto selection = *observation.selector_0xc98;
        selection.current_index_0x24 = std::bit_cast<std::int32_t>(
            fields.value_0xcbc);
        const auto selected = update_embedded_selection(
            selection, *observation.curve_0xbfc);
        fields.value_0xcbc = std::bit_cast<std::uint32_t>(selected);
        fields.value_0xd64 = fields.value_0xcbc;
        result.called_0x433210 = true;
    } else if (call_43b9a0) {
        if (call_4331e0_after_value) {
            const auto argument = std::bit_cast<std::int32_t>(
                globals.value_0x5093bc);
            fields.value_0xcbc = std::bit_cast<std::uint32_t>(argument);
            fields.value_0xd64 = fields.value_0xcbc;
            result.argument_0x4331e0 = argument;
            result.called_0x4331e0 = true;
        }
        // After optional 0x4331E0, native code reloads +0xD64 for 0x43B9B0.
        result.wrote_0x5093bc = true;
        result.output_0x5093bc = fields.value_0xd64;
    }
    fields.scalars.value_0xdec = pcar_callback_value_0x43b950(
        globals.value_0x5093c8);
    result.called_0x433250 = globals.gate_0x5093cc;
    fields.value_0x4 = pcar_status_flags_0x4385f0(
        fields.scalars.value_0xde4, fields.value_0xc34);
    result.status_flags = fields.value_0x4;
    result.complete = true;
    return result;
}

float pcar_global_scale_0x43b8c0(const float input,
                                 const float global_0x509358,
                                 const bool square_magnitude_0x5093fc) noexcept
{
    float selected = global_0x509358;
    if (square_magnitude_0x5093fc) {
        selected = std::abs(global_0x509358) * global_0x509358;
    }

    constexpr std::int32_t raw_bit_threshold = 0x4178E38E;
    if (std::bit_cast<std::int32_t>(input) <= raw_bit_threshold) {
        return selected;
    }

    constexpr float input_cap = std::bit_cast<float>(std::uint32_t{0x4231C71C});
    constexpr float slope = std::bit_cast<float>(std::uint32_t{0xBC8DC8DD});
    constexpr float intercept = std::bit_cast<float>(std::uint32_t{0x3FA27627});
    const float capped_input = input < input_cap ? input : input_cap;
    return ((capped_input * slope) + intercept) * selected;
}

float pcar_callback_value_0x43b950(const float global_0x5093c8) noexcept
{
    const float value = (global_0x5093c8 * -1.2F) + 1.1F;
    return clamp_finite(value, 0.0F, 1.0F);
}

std::uint32_t pcar_status_flags_0x4385f0(const float value_0xde4,
                                         const float value_0xc34) noexcept
{
    const auto signed_bits = [](const float value) {
        return std::bit_cast<std::int32_t>(value);
    };
    std::uint32_t flags = 0;
    if (signed_bits(value_0xde4) > 0x3DCCCCCD) {
        flags |= 1U;
    }
    if (signed_bits(value_0xc34) > 0x3ECCCCCD) {
        flags |= 2U;
    }
    return flags;
}

PcarTimestampUpdate update_pcar_timestamp(const std::uint32_t current_timestamp_bits,
                                          const bool enabled_0x509360,
                                          const float current_time) noexcept
{
    if (!enabled_0x509360) {
        return {0U, false};
    }
    if ((current_timestamp_bits & 0x7FFFFFFFU) == 0U) {
        return {std::bit_cast<std::uint32_t>(current_time), true};
    }

    constexpr float elapsed_threshold = std::bit_cast<float>(std::uint32_t{0x3DCCCCCD});
    constexpr std::uint32_t expired_sentinel_bits = 0x4CEB79A3;
    const float previous_time = std::bit_cast<float>(current_timestamp_bits);
    if (!(current_time - previous_time <= elapsed_threshold)) {
        return {expired_sentinel_bits, false};
    }
    return {current_timestamp_bits, false};
}

void accumulate_pcar_effect(PcarEffectAccumulators& accumulators,
                            const PcarEffectFields& fields) noexcept
{
    if (fields.disabled_0xe3d || fields.mode_0x4fc == 0U ||
        fields.mode_0x4fc == 3U) {
        return;
    }

    constexpr float kVectorEpsilon = 1.1920929e-7F;
    constexpr float kEffectMagnitude = 40000.0F;
    const auto add_effect = [&](const std::array<float, 3>& direction,
                                const std::array<float, 3>& point) {
        const float squared_length = direction[0] * direction[0] +
            direction[1] * direction[1] + direction[2] * direction[2];
        const float inverse_length = 1.0F / std::sqrt(squared_length);
        const std::array<float, 3> impulse{
            direction[0] * inverse_length * kEffectMagnitude,
            direction[1] * inverse_length * kEffectMagnitude,
            direction[2] * inverse_length * kEffectMagnitude};
        const float scaled_squared_length = impulse[0] * impulse[0] +
            impulse[1] * impulse[1] + impulse[2] * impulse[2];
        if (!(scaled_squared_length > kVectorEpsilon)) {
            return;
        }
        const std::array<float, 3> arm{
            point[0] - fields.center_0x5c[0],
            point[1] - fields.center_0x5c[1],
            point[2] - fields.center_0x5c[2]};

        for (std::size_t axis = 0; axis < 3; ++axis) {
            accumulators.force_0x24c[axis] += impulse[axis];
        }
        accumulators.moment_0x258[0] +=
            arm[1] * impulse[2] - arm[2] * impulse[1];
        accumulators.moment_0x258[1] +=
            arm[2] * impulse[0] - arm[0] * impulse[2];
        accumulators.moment_0x258[2] +=
            arm[0] * impulse[1] - arm[1] * impulse[0];
    };

    const std::uint32_t selector_bits = std::bit_cast<std::uint32_t>(fields.selector_0x48);
    constexpr std::uint32_t kLowBranchBoundary = 0xBF4CCCCD;
    constexpr std::uint32_t kHighBranchBoundary = 0x3E4CCCCD;
    if (selector_bits > kLowBranchBoundary) {
        std::array<float, 3> point = fields.center_0x5c;
        point[1] += 1.0F;
        add_effect(fields.direction_0x38, point);

        point[1] -= 4.0F;
        const std::array<float, 3> reverse_direction{
            -fields.direction_0x38[0],
            fields.direction_0x38[1],
            -fields.direction_0x38[2]};
        add_effect(reverse_direction, point);
    } else if (selector_bits < kHighBranchBoundary) {
        const std::array<float, 3> direction{
            -fields.direction_0x44[0],
            fields.selector_0x48,
            -fields.direction_0x44[2]};
        std::array<float, 3> point = fields.center_0x5c;
        point[1] += 1.0F;
        add_effect(direction, point);
    } else {
        const std::array<float, 3> direction{0.0F, kEffectMagnitude, 0.0F};
        const std::array<float, 3> point{
            fields.center_0x5c[0] + fields.direction_0x50[0] * 3.0F,
            fields.center_0x5c[1] + fields.direction_0x50[1] * 3.0F,
            fields.center_0x5c[2] + fields.direction_0x50[2] * 3.0F};
        add_effect(direction, point);
    }
}

PcarNearbyEventResult update_pcar_nearby_events(
    PcarNearbyEventState& state,
    const std::span<const PcarNearbyEntity> candidates) noexcept
{
    const auto dot = [](const std::array<float, 3>& left,
                        const std::array<float, 3>& right) {
        return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
    };
    const bool motion_gate =
        std::bit_cast<std::int32_t>(state.selector_bits_0x48) > 0 &&
        !(dot(state.motion_0x234, state.basis_0x50) <= 3.0F);
    if (!motion_gate) {
        state.event_state_0xed0 = 0U;
        state.prior_event_state_0xed4 = 0U;
        return {false, 0U, 0U, false};
    }

    if (!state.event_enabled_0xeec ||
        state.current_time - state.prior_event_time_0xee8 <= 0.5F) {
        return {false, state.event_state_0xed0, 0U, false};
    }

    state.event_state_0xed0 = 0U;
    constexpr std::uint32_t kPositiveLow = 0x3F333333;
    constexpr std::uint32_t kPositiveHigh = 0x40E00000;
    constexpr std::uint32_t kNegativeLow = 0xBF333333;
    constexpr std::uint32_t kNegativeHigh = 0xC0E00000;
    constexpr std::size_t kCandidateLimit = 16;
    const std::size_t count = candidates.size() < kCandidateLimit
        ? candidates.size() : kCandidateLimit;

    for (std::size_t index = 0; index < count; ++index) {
        const auto& candidate = candidates[index];
        if (!candidate.passes_callback_filter) {
            continue;
        }

        const std::array<float, 3> displacement{
            candidate.position_0x5c[0] - state.position_0x5c[0],
            candidate.position_0x5c[1] - state.position_0x5c[1],
            candidate.position_0x5c[2] - state.position_0x5c[2]};
        const float longitudinal = dot(state.basis_0x50, displacement);
        if (longitudinal <= -5.0F ||
            std::bit_cast<std::int32_t>(longitudinal) >= 0x40800000) {
            continue;
        }

        const float lateral = dot(state.basis_0x38, displacement);
        const std::uint32_t lateral_bits = std::bit_cast<std::uint32_t>(lateral);
        if (lateral <= 0.0F) {
            if (lateral_bits > kNegativeLow && lateral_bits < kNegativeHigh) {
                state.event_state_0xed0 |= 1U;
            }
        } else if (std::bit_cast<std::int32_t>(lateral) >
                       static_cast<std::int32_t>(kPositiveLow) &&
                   std::bit_cast<std::int32_t>(lateral) <
                       static_cast<std::int32_t>(kPositiveHigh)) {
            state.event_state_0xed0 |= 2U;
        }
    }

    if (state.event_state_0xed0 == state.prior_event_state_0xed4) {
        return {false, state.event_state_0xed0, 0U, false};
    }

    state.prior_event_state_0xed4 = state.event_state_0xed0;
    state.prior_event_time_0xee8 = state.current_time;
    std::uint32_t target_offset = 0xEE0U;
    bool target_present = state.target_0xee0_present;
    PcarNearbyCue cue = PcarNearbyCue::both;
    switch (state.event_state_0xed0) {
    case 0U:
        target_offset = 0xEE4U;
        target_present = state.target_0xee4_present;
        cue = PcarNearbyCue::clear;
        break;
    case 1U:
        target_offset = 0xED8U;
        target_present = state.target_0xed8_present;
        cue = PcarNearbyCue::left;
        break;
    case 2U:
        target_offset = 0xEDCU;
        target_present = state.target_0xedc_present;
        cue = PcarNearbyCue::right;
        break;
    default:
        break;
    }
    return {true, state.event_state_0xed0, target_offset, target_present, cue};
}

bool apply_pcar_nearby_notification(
    const std::span<std::byte> target_object,
    const PcarNearbyEventResult& result) noexcept
{
    if (!result.state_changed || !result.notification_target_present ||
        target_object.size() < 0x2cU) {
        return false;
    }
    target_object[0x29U] = std::byte{1};
    target_object[0x2bU] = std::byte{0};
    target_object[0x2aU] = std::byte{0};
    return true;
}

bool write_pcar_step_record(
    const std::span<std::byte> destination,
    const PcarStepRecordObservation& observation) noexcept
{
    if (destination.size() < pcar_step_record_bytes) {
        return false;
    }
    const std::array<std::uint32_t, 5> words{
        observation.backend_value_0x50,
        observation.backend_value_0x48,
        observation.object_value_0xddc,
        observation.object_value_0xea4,
        observation.object_value_0xea8,
    };
    for (std::size_t index = 0; index < words.size(); ++index) {
        for (std::size_t byte = 0; byte < sizeof(std::uint32_t); ++byte) {
            destination[index * sizeof(std::uint32_t) + byte] =
                static_cast<std::byte>((words[index] >> (byte * 8U)) & 0xffU);
        }
    }
    return true;
}

PcarCallbackPostIntegratorResult advance_pcar_callback_post_integrator(
    const std::span<std::byte> step_record_destination,
    PcarNearbyEventState& nearby_state,
    const PcarCallbackPostIntegratorObservation& observation) noexcept
{
    PcarCallbackPostIntegratorResult result{};
    if (!observation.integrator_reached) {
        return result;
    }

    result.reached = true;
    result.step_record_written = write_pcar_step_record(
        step_record_destination, observation.step_record);
    result.nearby_events = update_pcar_nearby_events(
        nearby_state, observation.nearby_candidates);
    result.nearby_notification_written = apply_pcar_nearby_notification(
        observation.notification_target_object, result.nearby_events);
    return result;
}

} // namespace viper
