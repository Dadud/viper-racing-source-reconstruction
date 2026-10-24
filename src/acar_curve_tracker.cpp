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

AcarCurveDistancePlan plan_acar_curve_distance(
    const AcarCurveDistanceObservation& observation)
{
    AcarCurveDistancePlan plan{};
    plan.cursor_node_address = observation.cursor_node_address;
    plan.fraction_0x08 = observation.fraction_0x08;
    plan.fdiv_compatibility_path_used =
        observation.fdiv_compatibility_enabled;
    auto current_address = observation.cursor_node_address;
    auto fraction = observation.fraction_0x08;

    if (current_address == 0U) {
        plan.segment_selector_called = true;
        const auto head_address = observation.circular_list_head_address;
        if (head_address == 0U) {
            plan.status = AcarCurveDistanceStatus::no_segment_found;
            plan.fraction_0x08 = 0.0F;
            plan.distance_along_curve = 0.0F;
            plan.output_written = true;
            return plan;
        }
        const auto* head = find_node(observation.captured_nodes, head_address);
        if (head == nullptr) {
            plan.status = AcarCurveDistanceStatus::node_capture_incomplete;
            return plan;
        }
        if (head->next_address == head_address) {
            current_address = head_address;
            plan.singleton_list_shortcut = true;
        } else {
            auto address = head_address;
            float best_distance = 3.4028235e+38F;
            for (std::size_t visited = 0U;
                 visited <= observation.captured_nodes.size(); ++visited) {
                const auto* node = find_node(observation.captured_nodes,
                                             address);
                if (node == nullptr) {
                    plan.status = AcarCurveDistanceStatus::
                        node_capture_incomplete;
                    return plan;
                }
                if (node->next_address == 0U) {
                    plan.status = AcarCurveDistanceStatus::
                        null_node_dereference;
                    return plan;
                }
                const auto* next = find_node(observation.captured_nodes,
                                             node->next_address);
                if (next == nullptr) {
                    plan.status = AcarCurveDistanceStatus::
                        node_capture_incomplete;
                    return plan;
                }

                const auto dx = observation.target_x
                    - node->position_x_0x04;
                const auto dy = observation.target_y
                    - node->position_y_0x08;
                const auto next_dx = observation.target_x
                    - next->position_x_0x04;
                const auto next_dy = observation.target_y
                    - next->position_y_0x08;
                const auto distance_current = static_cast<float>(
                    static_cast<long double>(dx) * dx
                    + static_cast<long double>(dy) * dy);
                const auto distance_next = static_cast<float>(
                    static_cast<long double>(next_dx) * next_dx
                    + static_cast<long double>(next_dy) * next_dy);
                const auto distance = distance_current <= distance_next
                    ? distance_current : distance_next;
                const auto current_tangent_dot = static_cast<float>(
                    static_cast<long double>(node->tangent_y_0x10) * dy
                    + static_cast<long double>(node->tangent_x_0x0c) * dx);
                const auto next_tangent_dot = static_cast<float>(
                    static_cast<long double>(next->tangent_x_0x0c) * next_dx
                    + static_cast<long double>(next->tangent_y_0x10) * next_dy);
                if (current_tangent_dot > 0.0F
                    && next_tangent_dot < 0.0F
                    && distance < best_distance) {
                    current_address = address;
                    best_distance = distance;
                }

                address = node->next_address;
                if (address == head_address) {
                    break;
                }
                if (visited == observation.captured_nodes.size()) {
                    plan.status = AcarCurveDistanceStatus::
                        node_capture_incomplete;
                    return plan;
                }
            }
        }
        if (current_address == 0U) {
            plan.status = AcarCurveDistanceStatus::no_segment_found;
            plan.fraction_0x08 = 0.0F;
            plan.distance_along_curve = 0.0F;
            plan.output_written = true;
            return plan;
        }
        plan.segment_selected = true;
        fraction = 0.0F;
    }

    const auto initial_address = current_address;
    const auto initial_fraction = fraction;
    const auto* current = find_node(observation.captured_nodes,
                                    current_address);
    if (current == nullptr) {
        plan.status = AcarCurveDistanceStatus::node_capture_incomplete;
        return plan;
    }
    auto distance_along = static_cast<float>(
        -static_cast<long double>(current->update_denominator_0x20)
            * static_cast<long double>(fraction));
    bool reached_distance_limit = false;
    bool converged = false;

    for (std::uint32_t iteration = 0U; iteration < 10U; ++iteration) {
        plan.iterations = iteration + 1U;
        if (current_address == 0U) {
            plan.status = AcarCurveDistanceStatus::null_node_dereference;
            return plan;
        }
        current = find_node(observation.captured_nodes, current_address);
        if (current == nullptr) {
            plan.status = AcarCurveDistanceStatus::node_capture_incomplete;
            return plan;
        }
        if (current->next_address == 0U) {
            plan.status = AcarCurveDistanceStatus::null_node_dereference;
            return plan;
        }
        const auto* next = find_node(observation.captured_nodes,
                                     current->next_address);
        if (next == nullptr) {
            plan.status = AcarCurveDistanceStatus::node_capture_incomplete;
            return plan;
        }

        const auto u = static_cast<long double>(fraction);
        const auto position_x = static_cast<float>(
            static_cast<long double>(next->position_x_0x04
                - current->position_x_0x04) * u
            + current->position_x_0x04);
        const auto position_y = static_cast<float>(
            static_cast<long double>(next->position_y_0x08
                - current->position_y_0x08) * u
            + current->position_y_0x08);
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
        const auto error = static_cast<float>(
            static_cast<long double>(observation.target_y - position_y)
                * tangent_y
            + static_cast<long double>(observation.target_x - position_x)
                * tangent_x);
        if (std::fabs(error) < 0.0005F) {
            converged = true;
            plan.converged_by_tangent_tolerance = true;
            break;
        }

        const auto correction = divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(error),
            static_cast<long double>(current->update_denominator_0x20),
            observation.fdiv_compatibility_enabled);
        fraction = static_cast<float>(correction
            + static_cast<long double>(fraction));

        auto fraction_bits = std::bit_cast<std::uint32_t>(fraction);
        std::size_t crossing_steps = 0U;
        while (fraction_bits < 0x80000000U
               && fraction_bits > 0x3f800000U) {
            if (current_address == 0U) {
                plan.status = AcarCurveDistanceStatus::null_node_dereference;
                return plan;
            }
            ++crossing_steps;
            if (crossing_steps > observation.captured_nodes.size() + 1U) {
                plan.status = AcarCurveDistanceStatus::node_capture_incomplete;
                return plan;
            }
            distance_along = static_cast<float>(
                static_cast<long double>(distance_along)
                + current->update_denominator_0x20);
            if (observation.maximum_distance < distance_along) {
                current_address = 0U;
                reached_distance_limit = true;
                plan.cursor_cleared_by_distance_limit = true;
                break;
            }
            current_address = current->next_address;
            current = current_address == 0U
                ? nullptr
                : find_node(observation.captured_nodes, current_address);
            if (current_address != 0U && current == nullptr) {
                plan.status = AcarCurveDistanceStatus::node_capture_incomplete;
                return plan;
            }
            fraction = static_cast<float>(
                static_cast<long double>(fraction) - 1.0L);
            fraction_bits = std::bit_cast<std::uint32_t>(fraction);
        }
        if (reached_distance_limit) {
            break;
        }
        if (fraction_bits > 0x80000000U) {
            fraction = 0.0F;
            plan.fraction_clamped_to_zero = true;
            break;
        }
    }

    if (!reached_distance_limit && !converged && plan.iterations == 10U) {
        plan.iteration_limit_reached = true;
    }
    if (current_address != 0U) {
        current = find_node(observation.captured_nodes, current_address);
        if (current == nullptr) {
            plan.status = AcarCurveDistanceStatus::node_capture_incomplete;
            return plan;
        }
        distance_along = static_cast<float>(
            static_cast<long double>(current->update_denominator_0x20)
                * static_cast<long double>(fraction)
            + distance_along);
        if (current_address == initial_address
            && fraction < initial_fraction) {
            current_address = 0U;
            plan.cursor_cleared_after_backward_update = true;
        }
    }

    plan.status = AcarCurveDistanceStatus::updated;
    plan.cursor_node_address = current_address;
    plan.fraction_0x08 = fraction;
    plan.distance_along_curve = distance_along;
    plan.output_written = true;
    return plan;
}

} // namespace viper
