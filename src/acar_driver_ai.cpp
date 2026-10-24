#include "viper/acar_driver_ai.hpp"

#include "viper/legacy_fdiv.hpp"

#include <bit>
#include <cmath>

namespace viper {
namespace {

bool signed_bits_less(const float value, const std::uint32_t boundary) noexcept
{
    return std::bit_cast<std::int32_t>(value) <
        std::bit_cast<std::int32_t>(boundary);
}

bool candidate_direction_update(AcarDriverAiState& state,
                                const std::uint32_t direction_bits,
                                const float score,
                                const std::uint32_t auxiliary_0,
                                const std::uint32_t auxiliary_1,
                                const bool mode) noexcept
{
    bool store = !state.directional_candidate_valid_0xef4;
    if (!store && ((state.directional_value_bits_0xedc ^ direction_bits)
                   & 0x80000000U) == 0U) {
        if (!mode) {
            store = state.directional_mode_0xeec ||
                std::fabs(state.directional_score_0xee0) < std::fabs(score);
        } else {
            store = state.directional_mode_0xeec &&
                std::fabs(score) >
                    std::fabs(state.directional_score_0xee0);
        }
    } else if (!store) {
        state.direction_changed_0xef5 = true;
    }

    if (store) {
        state.directional_value_bits_0xedc = direction_bits;
        state.directional_score_0xee0 = score;
        state.directional_aux_0xee4 = auxiliary_0;
        state.directional_aux_0xee8 = auxiliary_1;
        state.directional_mode_0xeec = mode;
        state.directional_candidate_valid_0xef4 = true;
    }
    return store;
}

bool candidate_forward_update(AcarDriverAiState& state,
                              const float value,
                              const std::uint32_t auxiliary) noexcept
{
    if (state.forward_candidate_valid_0xed0 &&
        state.forward_value_0xed4 < value) {
        state.forward_value_0xed4 = value;
        state.forward_aux_0xed8 = auxiliary;
        return true;
    }
    if (!state.forward_candidate_valid_0xed0) {
        state.forward_value_0xed4 = value;
        state.forward_aux_0xed8 = auxiliary;
        state.forward_candidate_valid_0xed0 = true;
        return true;
    }
    return false;
}

} // namespace

AcarDriverAiPlan update_acar_driver_ai_candidates(
    AcarDriverAiState& state,
    const AcarDriverAiObservation& observation) noexcept
{
    AcarDriverAiPlan plan{};
    plan.return_low_byte =
        static_cast<std::uint8_t>(observation.incoming_eax & 0xffU);
    plan.finite_inputs_assumed =
        std::isfinite(observation.velocity_x_0x234) &&
        std::isfinite(observation.velocity_z_0x23c) &&
        std::isfinite(observation.angle_gate_0xf1c) &&
        std::isfinite(observation.route_direction_0x04_0x08[0]) &&
        std::isfinite(observation.route_direction_0x04_0x08[1]) &&
        std::isfinite(observation.route_comparison_0x0c_0x10[0]) &&
        std::isfinite(observation.route_comparison_0x0c_0x10[1]);

    if (std::bit_cast<std::int32_t>(
            observation.profile_threshold_bits_0xf10) < 0x410e38e4) {
        plan.return_low_byte = 0U;
        return plan;
    }
    plan.profile_gate_passed = true;

    auto direction = observation.route_direction_0x04_0x08;
    auto comparison = observation.route_comparison_0x0c_0x10;
    const auto owner_matches = static_cast<std::int32_t>(
            observation.route_owner_0x00) == static_cast<std::int32_t>(
                observation.selected_driver_ordinal_0x510);
    if (!owner_matches) {
        direction[0] = -direction[0];
        direction[1] = -direction[1];
        comparison[0] = -comparison[0];
        comparison[1] = -comparison[1];
        plan.route_accessors_negated = true;
    }

    const auto speed = std::sqrt(
        observation.velocity_x_0x234 * observation.velocity_x_0x234 +
        observation.velocity_z_0x23c * observation.velocity_z_0x23c);
    if (std::fabs(speed) > 1.1920929e-07F) {
        plan.fdiv_compatibility_path_used =
            observation.fdiv_compatibility_enabled;
        plan.normalized_velocity_x = static_cast<float>(
            divide_with_vc98_fdiv_compatibility(
                observation.velocity_x_0x234, speed,
                observation.fdiv_compatibility_enabled));
        plan.normalized_velocity_z = static_cast<float>(
            divide_with_vc98_fdiv_compatibility(
                observation.velocity_z_0x23c, speed,
                observation.fdiv_compatibility_enabled));
    } else {
        plan.normalized_velocity_x = observation.velocity_x_0x234;
        plan.normalized_velocity_z = observation.velocity_z_0x23c;
    }

    // 0x42A630 maps the pair (x,z) to (z,-x) before projection.
    const auto rotated_x = plan.normalized_velocity_z;
    const auto rotated_z = -plan.normalized_velocity_x;
    plan.forward_projection = plan.normalized_velocity_x * direction[0] +
        plan.normalized_velocity_z * direction[1];
    plan.lateral_projection = rotated_x * direction[0] +
        rotated_z * direction[1];
    plan.lateral_magnitude = std::fabs(plan.lateral_projection);
    plan.comparison_score = rotated_z * comparison[1] +
        rotated_x * comparison[0];
    plan.forward_score = plan.normalized_velocity_x * comparison[0] +
        plan.normalized_velocity_z * comparison[1];

    bool directional_mode = false;
    if (plan.lateral_magnitude < 4.572F &&
        !signed_bits_less(plan.lateral_magnitude, 0x3fe7a5e4U) &&
        std::fabs(plan.forward_projection) < 5.36448F) {
        const auto forward_bits = std::bit_cast<std::uint32_t>(
            plan.forward_projection);
        directional_mode = forward_bits > 0xc00f0d84U ||
            (0xbf6a3d71U < forward_bits &&
             std::fabs(observation.angle_gate_0xf1c) <= 0.7F);
        const auto comparison_bits = std::bit_cast<std::uint32_t>(
            plan.forward_score);
        const bool suppress_directional_mode =
            comparison_bits < 0xc08f0d85U ||
            std::bit_cast<std::int32_t>(plan.forward_score) >
                0x40471c71;
        const auto registry_index =
            static_cast<std::int32_t>(observation.other_driver_index_0x00)
                == static_cast<std::int32_t>(
                    observation.selected_driver_ordinal_0x510)
            ? static_cast<std::int32_t>(
                observation.alternate_driver_index_0x02)
            : static_cast<std::int32_t>(
                observation.other_driver_index_0x00);
        plan.registry_lookup_index_0x41b050 = registry_index;
        if (directional_mode &&
            !observation.other_driver_registry_entry_present &&
            !suppress_directional_mode) {
            directional_mode = false;
        }
        plan.directional_helper_called = true;
        const auto previous_changed = state.direction_changed_0xef5;
        plan.directional_candidate_stored = candidate_direction_update(
            state, std::bit_cast<std::uint32_t>(plan.lateral_projection),
            plan.comparison_score,
            std::bit_cast<std::uint32_t>(plan.forward_projection),
            std::bit_cast<std::uint32_t>(plan.forward_score),
            directional_mode);
        plan.direction_change_flag_set = !previous_changed &&
            state.direction_changed_0xef5;
        plan.return_low_byte = 1U;
    }

    if (signed_bits_less(std::fabs(plan.forward_projection), 0x418b79fbU) &&
        signed_bits_less(plan.lateral_magnitude, 0x401e7ef9U) &&
        plan.forward_projection > 0.0F) {
        plan.forward_helper_called = true;
        plan.forward_candidate_stored = candidate_forward_update(
            state, plan.forward_projection,
            std::bit_cast<std::uint32_t>(plan.forward_score));
        plan.return_low_byte = 1U;
    }
    return plan;
}

AcarDriverCallbackPlan evaluate_acar_driver_callback(
    AcarDriverAiState& state,
    const AcarDriverAiObservation& observation,
    const bool fallback_helper_result) noexcept
{
    AcarDriverCallbackPlan plan{};
    plan.route_candidate_plan = update_acar_driver_ai_candidates(
        state, observation);
    if (plan.route_candidate_plan.return_low_byte != 0U) {
        plan.callback_return_value = true;
        return plan;
    }

    plan.fallback_helper_0x42b040_called = true;
    plan.fallback_helper_result = fallback_helper_result;
    plan.callback_return_value = fallback_helper_result;
    return plan;
}

AcarSecondaryCandidateResult update_acar_secondary_candidate(
    AcarFallbackState& state,
    const float candidate_range,
    const std::uint32_t candidate_value,
    const std::uint32_t candidate_endpoint) noexcept
{
    AcarSecondaryCandidateResult result{};
    bool replace = !state.candidate_valid_0xf08;
    if (state.candidate_valid_0xf08) {
        state.candidate_replaced_0xf09 = true;
        result.replaced_flag_set = true;
        replace = candidate_range < state.candidate_range_0xef8;
    }
    if (replace) {
        state.candidate_range_0xef8 = candidate_range;
        state.candidate_value_0xefc = candidate_value;
        state.candidate_endpoint_0xf04 = candidate_endpoint;
        state.candidate_valid_0xf08 = true;
        result.candidate_replaced = true;
    }
    return result;
}

AcarRecoveryUpdateResult apply_acar_recovery_state(
    AcarFallbackState& state) noexcept
{
    AcarRecoveryUpdateResult result{};
    state.copied_route_value_0xfd0 = state.route_value_0xf0c;
    state.damping_value_0xde4 = 0.1F;
    state.response_value_0xde8 = 1.0F;
    state.mode_value_0xde0 = 0.0F;
    state.mode_value_0xdec = 0.0F;

    const auto target = state.route_direction_0xf18 < 1 ? -1.0F : 1.0F;
    result.steering_delta = target - state.steering_value_0x500;
    if (result.steering_delta <= 0.24F) {
        if (std::bit_cast<std::uint32_t>(result.steering_delta) >
            0xbe75c28fU) {
            result.steering_delta = -0.24F;
        }
    } else {
        result.steering_delta = 0.24F;
    }
    result.requested_steering =
        state.steering_value_0x500 + result.steering_delta;
    if (result.requested_steering > 1.0F) {
        result.requested_steering = 1.0F;
    }
    if (0xbf800000U <
        std::bit_cast<std::uint32_t>(result.requested_steering)) {
        result.requested_steering = -1.0F;
    }
    result.applied_steering = result.requested_steering;
    result.wheel_pair_output =
        state.steering_scale_0xdcc * result.applied_steering;
    result.alternate_wheel_pair_selected =
        state.alternate_wheel_pair_gate_0xbe4;
    if (result.alternate_wheel_pair_selected) {
        state.wheel_pair_value_0x978 = -result.wheel_pair_output;
        state.wheel_pair_value_0xb1c = -result.wheel_pair_output;
    } else {
        state.wheel_pair_value_0x630 = result.wheel_pair_output;
        state.wheel_pair_value_0x7d4 = result.wheel_pair_output;
    }
    state.steering_value_0x500 = result.applied_steering;
    return result;
}

} // namespace viper
