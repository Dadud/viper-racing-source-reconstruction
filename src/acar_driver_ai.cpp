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

const AcarCurveNodeObservation* find_curve_node(
    const std::span<const AcarCurveNodeObservation> nodes,
    const std::uint32_t address) noexcept
{
    for (const auto& node : nodes) {
        if (node.address == address) {
            return &node;
        }
    }
    return nullptr;
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

AcarRouteFallbackPlan plan_acar_route_fallback(
    AcarFallbackState& state,
    const AcarRouteFallbackObservation& observation)
{
    AcarRouteFallbackPlan plan{};
    auto lookup_observation = observation.curve_lookup;
    lookup_observation.maximum_distance =
        observation.profile_threshold_0xf10 * 4.0F;
    plan.lookup_called = true;
    plan.curve_lookup = plan_acar_curve_lookup(lookup_observation);
    if (plan.curve_lookup.status != AcarCurveLookupStatus::completed &&
        plan.curve_lookup.status != AcarCurveLookupStatus::disabled) {
        plan.input_incomplete = true;
    }
    if (plan.curve_lookup.status != AcarCurveLookupStatus::completed ||
        !plan.curve_lookup.callback_return_value ||
        !plan.curve_lookup.distance_along_curve.has_value()) {
        return plan;
    }
    plan.curve_candidate_found = true;

    const auto* current = find_curve_node(
        observation.curve_lookup.captured_nodes,
        plan.curve_lookup.output_cursor_node_address);
    if (current == nullptr || current->next_address == 0U ||
        !current->route_scalar_0x3c.has_value()) {
        plan.input_incomplete = true;
        return plan;
    }
    const auto* next = find_curve_node(
        observation.curve_lookup.captured_nodes, current->next_address);
    if (next == nullptr || !next->route_scalar_0x3c.has_value()) {
        plan.input_incomplete = true;
        return plan;
    }
    plan.curve_node_inputs_complete = true;

    const auto fraction = plan.curve_lookup.output_fraction_0x08;
    const auto fraction2 = fraction * fraction;
    const auto h00 = (fraction * 2.0F - 3.0F) * fraction2 + 1.0F;
    const auto h01 = (fraction * -2.0F + 3.0F) * fraction2;
    const auto h10 = ((fraction - 2.0F) * fraction + 1.0F) * fraction;
    const auto h11 = (fraction - 1.0F) * fraction2;
    const auto curve_x =
        current->position_x_0x04 * h00 +
        current->tangent_x_0x0c * h10 +
        next->position_x_0x04 * h01 +
        next->tangent_x_0x0c * h11;
    const auto curve_y =
        current->position_y_0x08 * h00 +
        current->tangent_y_0x10 * h10 +
        next->position_y_0x08 * h01 +
        next->tangent_y_0x10 * h11;
    plan.curve_position_x = curve_x;
    plan.curve_position_y = curve_y;

    auto tangent_x = current->tangent_x_0x0c +
        (next->tangent_x_0x0c - current->tangent_x_0x0c) * fraction;
    auto tangent_y = current->tangent_y_0x10 +
        (next->tangent_y_0x10 - current->tangent_y_0x10) * fraction;
    const auto tangent_length = std::sqrt(
        tangent_x * tangent_x + tangent_y * tangent_y);
    if (std::fabs(tangent_length) > 1.1920929e-07F) {
        tangent_x = static_cast<float>(divide_with_vc98_fdiv_compatibility(
            tangent_x, tangent_length,
            observation.fdiv_compatibility_enabled));
        tangent_y = static_cast<float>(divide_with_vc98_fdiv_compatibility(
            tangent_y, tangent_length,
            observation.fdiv_compatibility_enabled));
    }
    const auto rotated_tangent_x = tangent_y;
    const auto rotated_tangent_y = -tangent_x;
    const auto lateral_error =
        (observation.target_route_values[0] - curve_x) *
            rotated_tangent_x +
        (observation.target_route_values[1] - curve_y) *
            rotated_tangent_y;
    const auto route_progress =
        observation.target_route_values[2] * tangent_x +
        observation.target_route_values[3] * tangent_y;
    const auto lateral_direction =
        observation.target_route_values[2] * rotated_tangent_x +
        observation.target_route_values[3] * rotated_tangent_y;
    const auto remaining_distance =
        observation.profile_threshold_0xf10 - route_progress;
    plan.route_lateral_error = lateral_error;
    plan.remaining_distance_0xf10 = remaining_distance;

    auto route_direction = observation.route_direction_0x04_0x08;
    if (static_cast<std::int32_t>(observation.route_owner_0x00) !=
        static_cast<std::int32_t>(observation.selected_driver_ordinal_0x510)) {
        route_direction[0] = -route_direction[0];
        route_direction[1] = -route_direction[1];
    }
    auto velocity_x = observation.velocity_x_0x234;
    auto velocity_z = observation.velocity_z_0x23c;
    const auto speed = std::sqrt(velocity_x * velocity_x + velocity_z * velocity_z);
    if (std::fabs(speed) > 1.1920929e-07F) {
        velocity_x = static_cast<float>(divide_with_vc98_fdiv_compatibility(
            velocity_x, speed, observation.fdiv_compatibility_enabled));
        velocity_z = static_cast<float>(divide_with_vc98_fdiv_compatibility(
            velocity_z, speed, observation.fdiv_compatibility_enabled));
    }
    plan.route_forward_projection =
        velocity_x * route_direction[0] + velocity_z * route_direction[1];

    const auto remaining_bits = std::bit_cast<std::int32_t>(remaining_distance);
    if (remaining_bits <= 0x3faaaaab ||
        !(plan.route_forward_projection > 8.9408F)) {
        return plan;
    }
    plan.distance_and_speed_gates_passed = true;
    plan.approach_ratio = static_cast<float>(
        divide_with_vc98_fdiv_compatibility(
            *plan.curve_lookup.distance_along_curve,
            remaining_distance, observation.fdiv_compatibility_enabled));
    plan.steering_candidate = plan.approach_ratio * lateral_direction +
        lateral_error;
    const auto next_scalar = *next->route_scalar_0x3c;
    plan.interpolated_route_scalar_0x3c =
        (next_scalar - *current->route_scalar_0x3c) * fraction +
        *current->route_scalar_0x3c;
    plan.lateral_tolerance_passed =
        std::fabs(plan.interpolated_route_scalar_0x3c + lateral_error) <
            observation.lateral_tolerance_0xf14;
    if (!plan.lateral_tolerance_passed) {
        return plan;
    }

    if (std::bit_cast<std::uint32_t>(route_progress) < 0xc1b1c71dU) {
        if (std::trunc(plan.approach_ratio) < 1.0F) {
            plan.secondary_candidate_rejected_below_one = true;
            return plan;
        }
        plan.secondary_candidate_branch_reached = true;
        plan.secondary_candidate_result = update_acar_secondary_candidate(
            state, plan.approach_ratio,
            std::bit_cast<std::uint32_t>(plan.steering_candidate),
            std::bit_cast<std::uint32_t>(plan.steering_candidate +
                plan.interpolated_route_scalar_0x3c));
    } else {
        plan.recovery_state_branch_reached = true;
        plan.recovery_state_result = apply_acar_recovery_state(state);
    }
    plan.returned_true = true;
    return plan;
}

AcarRouteSteeringPlan plan_acar_route_steering(
    const AcarRouteSteeringObservation& observation) noexcept
{
    AcarRouteSteeringPlan plan{};
    plan.fdiv_compatibility_path_used =
        observation.fdiv_compatibility_enabled;

    if (std::bit_cast<std::int32_t>(observation.profile_threshold_0xf10) >
        0x3ee38e39) {
        if (!observation.current_segment_scalar_0x34.has_value() ||
            !observation.next_segment_scalar_0x34.has_value()) {
            plan.input_incomplete = true;
        } else {
            const auto fraction = static_cast<long double>(
                observation.segment_fraction_0x1064);
            const auto interpolated = static_cast<float>(
                static_cast<long double>(
                    *observation.next_segment_scalar_0x34) * fraction +
                (1.0L - fraction) *
                    *observation.current_segment_scalar_0x34);
            plan.interpolated_segment_scalar = interpolated;
            const auto normalized = static_cast<float>(
                divide_with_vc98_fdiv_compatibility(
                    static_cast<long double>(interpolated),
                    static_cast<long double>(
                        observation.profile_threshold_0xf10),
                    observation.fdiv_compatibility_enabled));
            plan.normalized_route_scalar = normalized;
            plan.segment_scalar_below_two = normalized < 2.0F;
        }
    } else {
        plan.segment_scalar_below_two = false;
    }

    if (plan.segment_scalar_below_two.has_value()) {
        plan.route_drive_gate = *plan.segment_scalar_below_two &&
            !observation.steering_gate_suppressed_0x101d;
    }

    const auto route_distance = static_cast<float>(
        static_cast<long double>(observation.profile_threshold_0xf10) -
        static_cast<long double>(observation.route_offset_0xf84));
    plan.route_distance = route_distance;
    const auto distance_gate = route_distance > 8.888889F;
    if (observation.alternate_route_mode_0xf30 || distance_gate) {
        plan.update_branch_active = true;
    } else if (plan.route_drive_gate.has_value()) {
        plan.update_branch_active = *plan.route_drive_gate &&
            std::bit_cast<std::int32_t>(route_distance) > 0;
    } else {
        plan.input_incomplete = true;
    }

    if (!plan.update_branch_active.has_value()) {
        return plan;
    }
    if (!*plan.update_branch_active) {
        if (plan.segment_scalar_below_two.value_or(false) &&
            observation.steering_gate_suppressed_0x101d) {
            plan.message_call =
                AcarRouteSteeringMessageCall::message_0x4d5288;
        }
        return plan;
    }

    if (observation.alternate_route_mode_0xf30) {
        auto output = static_cast<float>(
            1.0L - static_cast<long double>(route_distance) * 0.225L);
        if (output < -1.0F) {
            output = -1.0F;
        } else if (output > 1.0F) {
            output = 1.0F;
        }
        plan.output_0 = output;
        plan.output_written = true;
        if (std::bit_cast<std::uint32_t>(output) > 0x80000000U) {
            plan.output_1 = -output;
            plan.output_0 = 0.0F;
            plan.early_return = true;
        }
        return plan;
    }

    plan.message_call = AcarRouteSteeringMessageCall::message_0x4d528c;
    if (route_distance > 6.6666665F) {
        if (!observation.route_reference_scalar_0xf58_plus_0x34.has_value()) {
            plan.input_incomplete = true;
            return plan;
        }
        plan.output_0 = *observation.route_reference_scalar_0xf58_plus_0x34;
        plan.output_1 = 1.0F;
        plan.output_written = true;
        plan.early_return = true;
        return plan;
    }
    if (!observation.route_reference_scalar_0xf58_plus_0x34.has_value()) {
        plan.input_incomplete = true;
        return plan;
    }
    const auto spread = static_cast<float>(
        static_cast<long double>(route_distance) * 0.15L);
    plan.output_1 = static_cast<float>(
        static_cast<long double>(
            *observation.route_reference_scalar_0xf58_plus_0x34) + spread);
    plan.output_0 = static_cast<float>(
        static_cast<long double>(
            *observation.route_reference_scalar_0xf58_plus_0x34) - spread);
    plan.output_written = true;
    return plan;
}

AcarForwardCandidateSteeringPlan plan_acar_forward_candidate_steering(
    const AcarForwardCandidateSteeringObservation& observation) noexcept
{
    AcarForwardCandidateSteeringPlan plan{};
    plan.fdiv_compatibility_path_used =
        observation.fdiv_compatibility_enabled;
    if (!observation.forward_candidate_valid_0xed0) {
        return plan;
    }

    const auto value = std::bit_cast<std::int32_t>(
        observation.forward_candidate_value_bits_0xed4);
    const auto auxiliary_abs =
        std::fabs(observation.forward_candidate_auxiliary_0xed8);
    if (value < 0x40a482be || !(auxiliary_abs > 0.0001F)) {
        plan.output_0 = observation.incoming_output_0 <= 0.9F
            ? 0.9F : observation.incoming_output_0;
        plan.output_written = true;
        plan.message_call = AcarRouteSteeringMessageCall::message_0x4d52bc;
        return plan;
    }

    if (value <= 0x408f0d84) {
        return plan;
    }
    plan.candidate_threshold_passed = true;
    const auto candidate = std::bit_cast<float>(
        observation.forward_candidate_value_bits_0xed4);
    const auto raw_negative_ratio = static_cast<float>(
        -divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(candidate) - 4.91744L,
            static_cast<long double>(
                observation.forward_candidate_auxiliary_0xed8),
            observation.fdiv_compatibility_enabled));
    plan.normalized_candidate = raw_negative_ratio;
    if (std::bit_cast<std::int32_t>(raw_negative_ratio) <= 0) {
        return plan;
    }
    plan.quotient_gate_passed = true;

    auto floor = static_cast<float>(
        (2.5L - static_cast<long double>(raw_negative_ratio)) * 0.4L);
    if (floor >= 0.0F) {
        if (std::bit_cast<std::int32_t>(floor) > 0x3f800000) {
            floor = 1.0F;
        }
    } else {
        floor = 0.0F;
    }
    plan.output_0 = observation.incoming_output_0 <= floor
        ? floor : observation.incoming_output_0;
    plan.output_written = true;
    plan.message_call = AcarRouteSteeringMessageCall::message_0x4d52bc;
    return plan;
}

AcarControlResponsePlan plan_acar_control_response(
    const AcarDriverAiState& driver_state,
    const AcarControlResponseObservation& observation) noexcept
{
    AcarControlResponsePlan plan{};
    auto response = observation.state_0xef0;
    if (driver_state.direction_changed_0xef5) {
        if (observation.steering_output_0 <= 0.8F) {
            plan.steering_output_0 = 0.8F;
        } else {
            plan.steering_output_0 = observation.steering_output_0;
        }
        response = static_cast<float>(
            static_cast<long double>(response) * 0.96L);
        plan.message_calls.push_back(
            AcarControlResponseMessageCall::message_0x4d52b8);
    } else if (!driver_state.directional_candidate_valid_0xef4 ||
               driver_state.directional_mode_0xeec) {
        response = static_cast<float>(
            static_cast<long double>(response) * 0.992L);
        plan.state_decay_branch_reached = true;
    } else {
        const auto directional_value = std::bit_cast<float>(
            driver_state.directional_value_bits_0xedc);
        if (((std::bit_cast<std::uint32_t>(-directional_value) ^
              std::bit_cast<std::uint32_t>(response)) & 0x80000000U) != 0U) {
            response = 0.0F;
        }

        const auto ee8_gate =
            driver_state.directional_aux_0xee8 <= 0xc0d55555U;
        const auto route_signs_match =
            ((std::bit_cast<std::uint32_t>(observation.lateral_value_0xf18) ^
              observation.lateral_sign_reference_bits_0xf20) &
             0x80000000U) == 0U;
        const auto route_lateral_gate =
            static_cast<long double>(observation.route_value_0xf14) -
                1.61925L <
            std::fabs(static_cast<long double>(
                observation.lateral_value_0xf18));
        const auto steering_signs_differ =
            ((observation.steering_relation_bits_0xf1c ^
              driver_state.directional_value_bits_0xedc) &
             0x80000000U) != 0U;
        if (ee8_gate && route_signs_match && route_lateral_gate &&
            steering_signs_differ) {
            plan.edge_brake_gate_passed = true;
            plan.steering_output_0 = 0.6F;
            plan.message_calls.push_back(
                AcarControlResponseMessageCall::message_0x4d52c0);
            response = static_cast<float>(
                static_cast<long double>(response) * 0.97L);
        }

        const auto opposite_scores =
            ((std::bit_cast<std::uint32_t>(
                  driver_state.directional_score_0xee0) ^
              driver_state.directional_value_bits_0xedc) &
             0x80000000U) != 0U;
        const auto small_lateral_values =
            std::fabs(directional_value) < 2.8575F &&
            std::fabs(std::bit_cast<float>(
                driver_state.directional_aux_0xee4)) < 4.6939197F;
        const auto slam_gate = opposite_scores || small_lateral_values;
        plan.lateral_slam_gate_passed = slam_gate;
        plan.state_0x1074 = 0.0F;
        if (!slam_gate) {
            plan.message_calls.push_back(
                AcarControlResponseMessageCall::message_0x4d52ac);
        } else {
            const auto score_magnitude = std::fabs(
                static_cast<long double>(driver_state.directional_score_0xee0));
            const auto excess = score_magnitude - 0.9525L;
            const auto scaled_excess = static_cast<float>(excess * 0.20997375L);
            float response_scale{};
            if (scaled_excess >= 0.0F) {
                if (std::bit_cast<std::int32_t>(scaled_excess) <=
                    0x3f800000) {
                    response_scale = static_cast<float>(
                        excess * 0.839895L +
                        (excess * -0.20997375L + 1.0L) * 0.5L);
                } else {
                    response_scale = 4.0F;
                }
            } else {
                response_scale = 0.5F;
            }

            plan.state_0x1074 = driver_state.directional_score_0xee0;
            const auto lateral_step =
                std::bit_cast<std::int32_t>(
                    driver_state.directional_value_bits_0xedc) < 1
                    ? 0.0029F : -0.0029F;
            response = static_cast<float>(
                static_cast<long double>(response_scale) * lateral_step +
                response);
            plan.lateral_slam_state_written = true;
            plan.message_calls.push_back(
                std::bit_cast<std::int32_t>(response) > 0
                    ? AcarControlResponseMessageCall::message_0x4d52b0
                    : AcarControlResponseMessageCall::message_0x4d52b4);
        }
    }

    plan.next_state_0xef0 = response;
    plan.accumulator_output_1 = static_cast<float>(
        static_cast<long double>(observation.accumulator_input_1) + response);
    plan.output_1_written = true;
    return plan;
}

AcarBehaviorControlChainPlan compose_acar_behavior_control_chain(
    const AcarDriverAiState& driver_state,
    const AcarBehaviorControlChainObservation& observation)
{
    AcarBehaviorControlChainPlan plan{};
    plan.local_output_0 = observation.initial_local_output_0;
    plan.local_output_1 = observation.initial_local_output_1;

    plan.helper_0x42a6d0_called = true;
    plan.route_steering_plan = plan_acar_route_steering(
        observation.route_steering);
    if (plan.route_steering_plan->output_0.has_value()) {
        plan.local_output_0 = *plan.route_steering_plan->output_0;
    }
    if (plan.route_steering_plan->output_1.has_value()) {
        plan.local_output_1 = *plan.route_steering_plan->output_1;
    }
    if (plan.route_steering_plan->input_incomplete &&
        !plan.route_steering_plan->output_written) {
        plan.input_incomplete = true;
        return plan;
    }

    auto response_observation = observation.control_response;
    response_observation.steering_output_0 = plan.local_output_1;
    response_observation.accumulator_input_1 = plan.local_output_0;
    plan.helper_0x42b570_called = true;
    plan.control_response_plan = plan_acar_control_response(
        driver_state, response_observation);
    if (plan.control_response_plan->steering_output_0.has_value()) {
        plan.local_output_1 =
            *plan.control_response_plan->steering_output_0;
    }
    plan.local_output_0 =
        plan.control_response_plan->accumulator_output_1;

    auto forward_observation = observation.forward_steering;
    forward_observation.incoming_output_0 = plan.local_output_1;
    plan.helper_0x42b450_called = true;
    plan.forward_steering_plan = plan_acar_forward_candidate_steering(
        forward_observation);
    if (plan.forward_steering_plan->output_0.has_value()) {
        plan.local_output_1 = *plan.forward_steering_plan->output_0;
    }

    if (!(plan.local_output_1 >= observation.clamp_value_0x1018)) {
        plan.local_output_1 = observation.clamp_value_0x1018;
    }
    plan.tail_call_order = {
        AcarBehaviorControlTailCall::helper_0x428cd0,
        AcarBehaviorControlTailCall::helper_0x428b40,
        AcarBehaviorControlTailCall::helper_0x428e20,
        AcarBehaviorControlTailCall::helper_0x433230,
        AcarBehaviorControlTailCall::helper_0x4331d0,
        AcarBehaviorControlTailCall::helper_0x4331c0,
    };
    plan.tail_reached = true;
    return plan;
}

AcarComposedDriverCallbackPlan evaluate_acar_driver_callback_with_fallback(
    AcarDriverAiState& driver_state,
    AcarFallbackState& fallback_state,
    const AcarDriverAiObservation& driver_observation,
    const AcarRouteFallbackObservation& fallback_observation)
{
    AcarComposedDriverCallbackPlan plan{};
    plan.route_candidate_plan = update_acar_driver_ai_candidates(
        driver_state, driver_observation);
    if (plan.route_candidate_plan.return_low_byte != 0U) {
        plan.callback_return_value = true;
        return plan;
    }

    plan.fallback_helper_0x42b040_called = true;
    plan.fallback_plan = plan_acar_route_fallback(
        fallback_state, fallback_observation);
    if (!plan.fallback_plan->input_incomplete) {
        plan.callback_return_value = plan.fallback_plan->returned_true;
    }
    return plan;
}

} // namespace viper
