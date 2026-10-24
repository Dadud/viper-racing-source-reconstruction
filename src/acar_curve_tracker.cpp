#include "viper/acar_curve_tracker.hpp"

#include "viper/legacy_fdiv.hpp"

#include <bit>
#include <cmath>

namespace viper {
namespace {

const AcarCurveNodeObservation* find_node(
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

float curve_divide(const float numerator, const float denominator,
                   const bool compatibility_enabled) noexcept
{
    return static_cast<float>(divide_with_vc98_fdiv_compatibility(
        static_cast<long double>(numerator),
        static_cast<long double>(denominator), compatibility_enabled));
}

} // namespace

AcarCurveTrackerPlan plan_acar_curve_tracker_update(
    const AcarCurveTrackerObservation& observation)
{
    AcarCurveTrackerPlan plan{};
    plan.segment_node_address = observation.segment_node_address;
    plan.fraction_0x08 = observation.fraction_0x08;
    plan.fdiv_compatibility_path_used =
        observation.fdiv_compatibility_enabled;

    auto current_address = observation.segment_node_address;
    auto fraction = observation.fraction_0x08;
    std::uint32_t previous_address = 0U;
    bool rejected = false;

    for (std::uint32_t iteration = 0U; iteration < 10U; ++iteration) {
        plan.iterations = iteration + 1U;
        if (current_address == 0U) {
            plan.status = AcarCurveTrackerStatus::null_node_dereference;
            return plan;
        }
        const auto* current = find_node(observation.captured_nodes,
                                        current_address);
        if (current == nullptr) {
            plan.status = AcarCurveTrackerStatus::node_capture_incomplete;
            return plan;
        }
        if (current->next_address == 0U) {
            plan.status = AcarCurveTrackerStatus::null_node_dereference;
            return plan;
        }
        const auto* next = find_node(observation.captured_nodes,
                                     current->next_address);
        if (next == nullptr) {
            plan.status = AcarCurveTrackerStatus::node_capture_incomplete;
            return plan;
        }

        const auto u = static_cast<long double>(fraction);
        const float h00 = static_cast<float>(
            (u * 2.0L - 3.0L) * u * u + 1.0L);
        const float h01 = static_cast<float>(
            (u * -2.0L + 3.0L) * u * u);
        const float h10 = static_cast<float>(
            ((u - 2.0L) * u + 1.0L) * u);
        const float h11 = static_cast<float>((u - 1.0L) * u * u);

        const auto raw_tangent_x = static_cast<float>(
            static_cast<long double>(next->tangent_x_0x0c
                - current->tangent_x_0x0c) * u
            + current->tangent_x_0x0c);
        const auto raw_tangent_y = static_cast<float>(
            static_cast<long double>(next->tangent_y_0x10
                - current->tangent_y_0x10) * u
            + current->tangent_y_0x10);
        const auto tangent_length = std::sqrt(
            static_cast<long double>(raw_tangent_x) * raw_tangent_x
            + static_cast<long double>(raw_tangent_y) * raw_tangent_y);
        const auto tangent_x = curve_divide(raw_tangent_x,
            static_cast<float>(tangent_length),
            observation.fdiv_compatibility_enabled);
        const auto tangent_y = curve_divide(raw_tangent_y,
            static_cast<float>(tangent_length),
            observation.fdiv_compatibility_enabled);

        const auto position_x = static_cast<float>(
            static_cast<long double>(next->tangent_x_0x0c) * h11
            + static_cast<long double>(next->position_x_0x04) * h01
            + static_cast<long double>(current->tangent_x_0x0c) * h10
            + static_cast<long double>(current->position_x_0x04) * h00);
        const auto position_y = static_cast<float>(
            static_cast<long double>(next->tangent_y_0x10) * h11
            + static_cast<long double>(next->position_y_0x08) * h01
            + static_cast<long double>(current->tangent_y_0x10) * h10
            + static_cast<long double>(current->position_y_0x08) * h00);
        const auto dx = observation.target_x_0x18 - position_x;
        const auto dy = observation.target_y_0x1c - position_y;
        const auto distance_squared = static_cast<float>(
            static_cast<long double>(dx) * dx
            + static_cast<long double>(dy) * dy);
        plan.last_distance_squared = distance_squared;

        if (distance_squared > 22500.0F) {
            rejected = true;
            plan.distance_rejected = true;
            break;
        }

        const auto tangent_error = static_cast<float>(
            static_cast<long double>(dx) * tangent_x
            + static_cast<long double>(dy) * tangent_y);
        plan.last_tangent_error = tangent_error;
        if (std::fabs(tangent_error) < 0.0005F) {
            plan.converged_by_tangent_tolerance = true;
            break;
        }

        const auto correction = divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(tangent_error),
            static_cast<long double>(current->update_denominator_0x20),
            observation.fdiv_compatibility_enabled);
        auto updated_fraction_x87 = correction + u;
        fraction = static_cast<float>(updated_fraction_x87);

        auto crossed_previous = previous_address;
        if (updated_fraction_x87 > 1.0L) {
            std::size_t crossing_steps = 0U;
            do {
                ++crossing_steps;
                if (crossing_steps > observation.captured_nodes.size() + 1U) {
                    // A non-finite fraction on a cyclic native list could
                    // make the original inner walk nonterminating. Bound the
                    // portable projection to the captured link set.
                    plan.status = AcarCurveTrackerStatus::
                        node_capture_incomplete;
                    return plan;
                }
                crossed_previous = current_address;
                fraction = static_cast<float>(
                    static_cast<long double>(fraction) - 1.0L);
                current_address = current->next_address;
                plan.fraction_walked_forward = true;
                if (current_address == 0U) {
                    plan.status = AcarCurveTrackerStatus::
                        null_node_dereference;
                    return plan;
                }
                current = find_node(observation.captured_nodes,
                                    current_address);
                if (current == nullptr) {
                    plan.status = AcarCurveTrackerStatus::
                        node_capture_incomplete;
                    return plan;
                }
                updated_fraction_x87 = static_cast<long double>(fraction);
            } while (updated_fraction_x87 > 1.0L);
        }
        previous_address = crossed_previous;

        const auto fraction_bits = std::bit_cast<std::uint32_t>(fraction);
        if (fraction_bits > 0x80000000U) {
            if (crossed_previous == 0U) {
                fraction = 0.0F;
                rejected = true;
                break;
            }
            fraction = static_cast<float>(
                static_cast<long double>(fraction) + 1.0L);
            current_address = crossed_previous;
            previous_address = 0U;
            plan.fraction_walked_backward = true;
            if (fraction < 0.0F) {
                fraction = 0.0F;
            }
        }
    }

    if (!plan.distance_rejected && !rejected
        && !plan.converged_by_tangent_tolerance
        && plan.iterations == 10U) {
        plan.iteration_limit_reached = true;
    }
    plan.status = AcarCurveTrackerStatus::updated;
    plan.segment_node_address = current_address;
    plan.fraction_0x08 = fraction;
    plan.segment_valid = !rejected;
    plan.output_written = true;
    return plan;
}

} // namespace viper
