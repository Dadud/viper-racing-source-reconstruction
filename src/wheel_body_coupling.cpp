#include "viper/wheel_body_coupling.hpp"

#include "viper/legacy_fdiv.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace viper {

namespace {

using OrdinalPairPointer = std::optional<std::size_t>;

int compare_ordinal_pair_records(
    const OrdinalPairPointer& first,
    const OrdinalPairPointer& second,
    const std::span<const NcarOrdinalPairRecord> records) noexcept
{
    // The callback's null path compares the first pointer with 1, placing the
    // single diagonal null after every non-null pair pointer.
    if (!first) return 1;
    if (!second) return -1;

    const auto& a = records[*first];
    const auto& b = records[*second];
    // The x87 callback stores the difference to a float stack temporary
    // before its comparisons. Preserve the native subtraction/addition order;
    // binary64 represents each finite-float product exactly. Its 53-bit
    // intermediate precision can still differ from x87 extended precision
    // for adversarial cancellation, so bit-exact parity remains unverified.
    const auto ax = static_cast<double>(a.position_delta_x_0x04);
    const auto az = static_cast<double>(a.position_delta_z_0x08);
    const auto bx = static_cast<double>(b.position_delta_x_0x04);
    const auto bz = static_cast<double>(b.position_delta_z_0x08);
    const auto difference = static_cast<float>(
        ((ax * ax - bx * bx) - bz * bz) + az * az);
    constexpr auto tolerance = std::bit_cast<float>(0x3DCCCCCDU);
    if (std::isnan(difference)) return 0;
    if (difference >= tolerance) return 1;
    if (std::fabs(difference) < tolerance) return 0;
    return -1;
}

void shortsort_ordinal_pair_pointers(
    std::vector<OrdinalPairPointer>& values,
    std::size_t first,
    std::size_t last,
    const std::span<const NcarOrdinalPairRecord> records)
{
    while (first < last) {
        auto maximum = first;
        for (auto scan = first + 1U; scan <= last; ++scan) {
            if (compare_ordinal_pair_records(
                    values[scan], values[maximum], records) > 0) {
                maximum = scan;
            }
        }
        std::swap(values[maximum], values[last]);
        --last;
    }
}

void quicksort_ordinal_pair_pointers(
    std::vector<OrdinalPairPointer>& values,
    const std::span<const NcarOrdinalPairRecord> records)
{
    if (values.size() < 2U) return;
    struct Range { std::size_t first; std::size_t last; };
    std::vector<Range> pending;
    std::size_t first = 0U;
    std::size_t last = values.size() - 1U;
    for (;;) {
        const auto count = last - first + 1U;
        if (count > 8U) {
            std::swap(values[first + (count >> 1U)], values[first]);
            auto scan_left = first;
            auto scan_right = last + 1U;
            for (;;) {
                do {
                    ++scan_left;
                    if (scan_left > last) break;
                } while (compare_ordinal_pair_records(
                             values[scan_left], values[first], records) < 1);

                do {
                    --scan_right;
                    if (scan_right <= first) break;
                } while (compare_ordinal_pair_records(
                             values[scan_right], values[first], records) >= 0);

                if (scan_left <= scan_right) {
                    std::swap(values[scan_left], values[scan_right]);
                    continue;
                }
                break;
            }

            std::swap(values[first], values[scan_right]);
            const auto left_measure =
                static_cast<std::ptrdiff_t>(scan_right) - 1 -
                static_cast<std::ptrdiff_t>(first);
            const auto right_measure =
                static_cast<std::ptrdiff_t>(last) -
                static_cast<std::ptrdiff_t>(scan_left);
            if (left_measure < right_measure) {
                if (scan_left < last) pending.push_back({scan_left, last});
                if (first + 1U < scan_right) {
                    last = scan_right - 1U;
                    continue;
                }
            } else {
                if (first + 1U < scan_right) {
                    pending.push_back({first, scan_right - 1U});
                }
                first = scan_left;
                if (scan_left < last) continue;
            }
        } else {
            shortsort_ordinal_pair_pointers(values, first, last, records);
        }

        if (pending.empty()) return;
        const auto next = pending.back();
        pending.pop_back();
        first = next.first;
        last = next.last;
    }
}

} // namespace

NcarOrdinalPairLookupResult resolve_ncar_ordinal_pair_lookup(
    const std::span<const NcarOrdinalPairRowInput> rows,
    const std::uint32_t ordinal)
{
    NcarOrdinalPairLookupResult result{};
    result.row_ordinal = ordinal;
    if (rows.empty() || ordinal >= rows.size() ||
        rows.size() > static_cast<std::size_t>(
            std::numeric_limits<std::int16_t>::max())) {
        return result;
    }

    const auto row_values = [rows](const std::size_t index) {
        const auto& row = rows[index];
        if (!row.registered) return NcarOrdinalPairRowInput{};
        return row;
    };
    const auto selected_row = static_cast<std::size_t>(ordinal);
    const auto selected_values = row_values(selected_row);
    std::vector<NcarOrdinalPairRecord> records;
    records.reserve(rows.size() - 1U);
    std::vector<OrdinalPairPointer> selected_list(rows.size());
    for (std::size_t partner = 0U; partner < rows.size(); ++partner) {
        if (partner == selected_row) continue;
        const auto partner_values = row_values(partner);
        const auto record_index = records.size();
        if (selected_row < partner) {
            records.push_back({
                static_cast<std::int16_t>(selected_row),
                static_cast<std::int16_t>(partner),
                partner_values.position_x_0x5c - selected_values.position_x_0x5c,
                partner_values.position_z_0x64 - selected_values.position_z_0x64,
                partner_values.velocity_x_0x234 - selected_values.velocity_x_0x234,
                partner_values.velocity_z_0x23c - selected_values.velocity_z_0x23c});
        } else {
            records.push_back({
                static_cast<std::int16_t>(partner),
                static_cast<std::int16_t>(selected_row),
                selected_values.position_x_0x5c - partner_values.position_x_0x5c,
                selected_values.position_z_0x64 - partner_values.position_z_0x64,
                selected_values.velocity_x_0x234 - partner_values.velocity_x_0x234,
                selected_values.velocity_z_0x23c - partner_values.velocity_z_0x23c});
        }
        selected_list[partner] = record_index;
    }

    const auto record_span = std::span<const NcarOrdinalPairRecord>(records);
    quicksort_ordinal_pair_pointers(selected_list, record_span);
    result.sorted_partner_ordinals.reserve(selected_list.size());
    for (const auto& pointer : selected_list) {
        if (!pointer) {
            result.sorted_partner_ordinals.emplace_back(std::nullopt);
            continue;
        }
        const auto& record = records[*pointer];
        const auto partner = record.endpoint_a_ordinal_0x00 == ordinal
            ? record.endpoint_b_ordinal_0x02
            : record.endpoint_a_ordinal_0x00;
        result.sorted_partner_ordinals.emplace_back(
            static_cast<std::uint32_t>(partner));
    }
    if (ordinal < selected_list.size() && selected_list[ordinal].has_value()) {
        result.record = records[*selected_list[ordinal]];
    }
    result.input_complete = true;
    return result;
}

NcarIntegratorPrefixResult apply_ncar_integrator_prefix(
    BodyIntegrationKinematicsInput& kinematics,
    NcarForceMomentAccumulator& accumulator,
    const NcarIntegratorPrefixInput& input,
    const std::array<std::uint8_t, 4>& contact_bytes) noexcept
{
    NcarIntegratorPrefixResult result{};
    if (input.state_0x4fc == 0U) {
        result.initialization_reset_reached = true;
        kinematics.linear_velocity.x = 0.0F;
        kinematics.linear_velocity.z = 0.0F;
    }
    if (!input.force_gate_0xe3d) {
        return result;
    }

    const auto contact_count = std::accumulate(
        contact_bytes.begin(), contact_bytes.end(), std::uint32_t{0});
    result.contact_virtual_callback_0x50_reached = contact_count > 2U;
    const auto point = transform_body_local_point({
        {0.0F, 2.0F, 0.0F},
        kinematics.orientation_matrix_0x0e,
        kinematics.position});
    result.application_point = point;
    result.point_velocity = derive_body_point_velocity_0x43e9b0({
        kinematics.position,
        point,
        kinematics.linear_velocity,
        kinematics.angular_velocity});
    result.force = {
        0.0F,
        input.mass_0x7c * result.point_velocity.y * -4.4145F,
        0.0F};
    result.initial_force_call_0x43e6f0_reached = true;
    result.force_accumulated = accumulate_ncar_force_moment(
        accumulator,
        {result.force.x, result.force.y, result.force.z},
        {point.x, point.y, point.z},
        {kinematics.position.x, kinematics.position.y, kinematics.position.z});
    return result;
}

NcarIntegratorTimerResult advance_ncar_integrator_timer(
    NcarIntegratorTimerState& state,
    const NcarIntegratorTimerInput& input)
{
    NcarIntegratorTimerResult result{};
    constexpr auto timer_tick_seconds = std::bit_cast<float>(0x3C83126FU);
    constexpr auto six_point_two = std::bit_cast<float>(0x40C66666U);
    constexpr auto six_seconds_bits = std::uint32_t{0x40C00000U};
    constexpr auto ramp_square_scale = std::bit_cast<float>(0x3CE38E39U);
    constexpr auto ramp_floor = std::bit_cast<float>(0x3E19999AU);
    constexpr auto ramp_scale = std::bit_cast<float>(0x3F5E9BD3U);
    constexpr auto record_radius_squared = std::bit_cast<float>(0x41CE7381U);

    result.vehicle_ordinal_0x510 = input.vehicle_ordinal_0x510;
    const auto now = static_cast<long double>(input.timer_ticks_0x508774) *
        static_cast<long double>(timer_tick_seconds);
    const auto elapsed_extended = now -
        static_cast<long double>(state.record_time_0xe44);
    if (elapsed_extended >= static_cast<long double>(six_point_two)) {
        return result;
    }

    result.time_window_reached = true;
    const auto elapsed = static_cast<float>(elapsed_extended);
    result.elapsed_time = elapsed;
    if (std::bit_cast<std::int32_t>(elapsed) <
        static_cast<std::int32_t>(six_seconds_bits)) {
        result.quadratic_ramp_branch = true;
        const auto wide_elapsed = static_cast<long double>(elapsed);
        const auto ramp = (wide_elapsed * wide_elapsed * ramp_square_scale +
                           ramp_floor) * ramp_scale;
        state.ramp_0x50c = static_cast<float>(ramp);
    } else {
        state.ramp_0x50c = 1.0F;
    }

    result.ordinal_record_lookup_reached = true;
    auto lookup_resolved = input.record_lookup_resolved;
    auto record_nonnull = input.record_nonnull;
    auto record_x = input.record_x_0x04;
    auto record_y = input.record_y_0x08;
    if (input.ordinal_rows_resolved) {
        const auto lookup = resolve_ncar_ordinal_pair_lookup(
            input.ordinal_rows, input.vehicle_ordinal_0x510);
        if (!lookup.input_complete) {
            result.input_complete = false;
            return result;
        }
        lookup_resolved = true;
        record_nonnull = lookup.record.has_value();
        if (lookup.record) {
            record_x = lookup.record->position_delta_x_0x04;
            record_y = lookup.record->position_delta_z_0x08;
        }
    }
    if (!lookup_resolved) {
        result.input_complete = false;
        return result;
    }
    if (!record_nonnull) {
        return result;
    }
    const auto record_distance_squared =
        static_cast<long double>(record_x) * record_x +
        static_cast<long double>(record_y) * record_y;
    if (record_distance_squared >=
            static_cast<long double>(record_radius_squared) &&
        !std::isnan(record_distance_squared)) {
        return result;
    }

    result.record_radius_gate_passed = true;
    state.record_time_0xe44 = static_cast<float>(
        static_cast<long double>(state.record_time_0xe44) + timer_tick_seconds);
    result.record_time_advanced = true;
    return result;
}

NcarIntegratorEarlyBoundsResult plan_ncar_integrator_early_bounds(
    const PhysicsVector3& position,
    const std::uint32_t vehicle_ordinal_0x510) noexcept
{
    const std::array<std::uint32_t, 3> bits{
        std::bit_cast<std::uint32_t>(position.x),
        std::bit_cast<std::uint32_t>(position.y),
        std::bit_cast<std::uint32_t>(position.z)};
    const auto below_raw_limits =
        bits[0] < 0xC59C4000U &&
        std::bit_cast<std::int32_t>(bits[0]) < 0x459C4000 &&
        bits[1] < 0xC47A0000U &&
        std::bit_cast<std::int32_t>(bits[1]) < 0x453B8000 &&
        bits[2] < 0xC59C4000U &&
        std::bit_cast<std::int32_t>(bits[2]) < 0x459C4000;

    NcarIntegratorEarlyBoundsResult result{};
    result.within_bounds = below_raw_limits;
    result.callback_0x508a04_slot_0x3c_reached = !below_raw_limits;
    if (result.callback_0x508a04_slot_0x3c_reached) {
        result.callback_vehicle_ordinal_0x510 = vehicle_ordinal_0x510;
    }
    return result;
}

NcarIntegratorCurveGateResult advance_ncar_integrator_curve_gate(
    NcarIntegratorCurveGateState& state,
    const NcarIntegratorCurveGateInput& input) noexcept
{
    NcarIntegratorCurveGateResult result{};
    constexpr std::int32_t helper_threshold_bits = 0x3C23D70A;
    if (input.parent_gate_0xc3d != 0U &&
        std::bit_cast<std::int32_t>(input.parent_value_bits_0xc7c) <
            helper_threshold_bits) {
        result.curve_helper_0x4402d0_reached = true;
        if (input.curve_gate_0x41 != 0U) {
            state.parent_flag_0xc3c = 1U;
            result.curve_byte_0x40_written = true;
            result.parent_flag_0xc3c_written = true;
        }
    }
    if (state.parent_flag_0xc3c != 0U) {
        state.value_0xde0 = 1.0F;
        result.setter_0x4331a0_reached = true;
    }
    return result;
}

NcarIntegratorSignalResult advance_ncar_integrator_signal(
    NcarIntegratorSignalState& state,
    const NcarIntegratorSignalInput& input,
    const std::uint8_t copied_flag_0xc3c) noexcept
{
    NcarIntegratorSignalResult result{};
    const float scaled_callback = input.callback_value_0x339 * input.multiplier_0xdd0;
    state.stored_value_0xddc = scaled_callback;
    state.copied_flag_0xe49 = copied_flag_0xc3c;

    constexpr std::int32_t ramp_threshold_bits = 0x44FA0000;
    if (input.ramp_gate_0xbe4 != 0U &&
        std::bit_cast<std::int32_t>(input.previous_value_bits_0xd70) >
            ramp_threshold_bits) {
        state.value_bits_0xc7c = 0x3F800000U;
        result.ramp_setter_0x43f8e0_reached = true;
    }

    constexpr float ramp_weight = std::bit_cast<float>(0x3E199998U);
    constexpr float retained_weight = std::bit_cast<float>(0x3F59999AU);
    const float value_0xc7c = std::bit_cast<float>(state.value_bits_0xc7c);
    const float clamp_bound = state.limit_scale_0xc8c * value_0xc7c;
    auto delta = static_cast<float>(
        (static_cast<double>(input.callback_value_0xc84) -
         static_cast<double>(input.callback_value_0xc80)) *
        static_cast<double>(state.callback_scale_0xc90));
    if (clamp_bound < delta) {
        delta = clamp_bound;
    } else if (delta < -clamp_bound) {
        delta = -clamp_bound;
    }
    state.smoothed_value_0xc94 = static_cast<float>(
        static_cast<double>(delta) * static_cast<double>(ramp_weight) +
        static_cast<double>(state.smoothed_value_0xc94) *
            static_cast<double>(retained_weight));
    const auto paired_value = static_cast<float>(
        static_cast<double>(value_0xc7c) * static_cast<double>(value_0xc7c) *
        static_cast<double>(state.smoothed_value_0xc94));

    result.helper_0x43f830_reached = true;
    result.paired_setters_reached = true;
    result.clamp_bound = clamp_bound;
    result.smoothed_value = state.smoothed_value_0xc94;
    result.paired_setter_value = paired_value;
    result.paired_setter_opposite = -paired_value;
    return result;
}

NcarIntegratorWheelCallbackSetup
initialize_ncar_integrator_wheel_callback_setup(
    const NcarIntegratorWheelCallbackSetupInput& input) noexcept
{
    const float width = input.source_width_0x58 * 0.0254F;
    const float ratio = input.source_ratio_0x5c * 0.01F;
    return {
        (1.0F - ratio) * width,
        -(ratio * width),
        input.source_value_0x12c * 0.0174F};
}

std::optional<NcarIntegratorWheelCallbackSetup>
initialize_ncar_integrator_wheel_callback_setup_from_runtime_record(
    const std::span<const std::byte> runtime_record) noexcept
{
    constexpr std::size_t width_offset = 0x58U;
    constexpr std::size_t ratio_offset = 0x5cU;
    constexpr std::size_t value_offset = 0x12cU;
    constexpr std::size_t required_size = value_offset + sizeof(std::uint32_t);
    if (runtime_record.size() < required_size) return std::nullopt;

    const auto read_u32_le = [&runtime_record](const std::size_t offset) {
        std::uint32_t value{};
        for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
            value |= static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>(runtime_record[offset + byte]))
                << (byte * 8U);
        }
        return value;
    };
    const NcarIntegratorWheelCallbackSetupInput input{
        std::bit_cast<float>(read_u32_le(width_offset)),
        std::bit_cast<float>(read_u32_le(ratio_offset)),
        std::bit_cast<float>(read_u32_le(value_offset))};
    return initialize_ncar_integrator_wheel_callback_setup(input);
}

NcarWheelVertexAnchorResult select_ncar_wheel_vertex_anchors(
    const NcarWheelVertexAnchorInput& input) noexcept
{
    NcarWheelVertexAnchorResult result{};
    const float width = input.source_width_0x58 * 0.0254F;
    const float ratio = input.source_ratio_0x5c * 0.01F;
    const float y = input.source_offset_0x128 * -0.0254F;
    const std::array<float, 2> axle_z{
        (1.0F - ratio) * width,
        -(ratio * width)};
    constexpr std::array<float, 4> x{-0.4F, 0.4F, -0.4F, 0.4F};
    for (std::size_t wheel = 0; wheel < result.candidate_positions.size(); ++wheel) {
        const auto z = axle_z[wheel < 2U ? 0U : 1U];
        result.candidate_positions[wheel] = {x[wheel], y, z};
        if (!input.model_vertex_anchor_enabled) {
            result.sources[wheel] = NcarWheelAnchorSource::shared_zero_vector;
            result.selected_anchor_positions[wheel] = {0.0F, 0.0F, 0.0F};
            continue;
        }
        result.sources[wheel] = NcarWheelAnchorSource::model_vertex;
        float nearest_distance = 100000.0F;
        for (std::size_t vertex = 0; vertex < input.model_vertices.size(); ++vertex) {
            const auto& point = input.model_vertices[vertex].fields;
            const float dx = x[wheel] - point[0];
            const float dy = y - point[1];
            const float dz = z - point[2];
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < nearest_distance) {
                nearest_distance = distance;
                result.vertex_indices[wheel] = vertex;
            }
        }
        result.nearest_distances[wheel] = nearest_distance;
        if (result.vertex_indices[wheel].has_value()) {
            const auto& selected = input.model_vertices[
                *result.vertex_indices[wheel]].fields;
            result.selected_anchor_positions[wheel] = {
                selected[0], selected[1], selected[2]};
        }
    }
    return result;
}

bool ncar_wheel_vertex_anchor_result_complete(
    const NcarWheelVertexAnchorResult& result) noexcept
{
    for (std::size_t wheel = 0U; wheel < result.sources.size(); ++wheel) {
        if (result.sources[wheel] == NcarWheelAnchorSource::model_vertex) {
            if (!result.vertex_indices[wheel].has_value()) return false;
        } else if (result.sources[wheel] ==
                       NcarWheelAnchorSource::shared_zero_vector) {
            if (result.vertex_indices[wheel].has_value() ||
                result.selected_anchor_positions[wheel] !=
                    std::array<float, 3>{0.0F, 0.0F, 0.0F}) return false;
        } else {
            return false;
        }
    }
    return true;
}

std::optional<NcarWheelVertexAnchorResult>
select_ncar_wheel_vertex_anchors_from_runtime_record(
    const std::span<const std::byte> runtime_record,
    const std::span<const ModelVertex> model_vertices,
    const bool model_vertex_anchor_enabled) noexcept
{
    constexpr std::size_t width_offset = 0x58U;
    constexpr std::size_t ratio_offset = 0x5cU;
    constexpr std::size_t body_offset = 0x128U;
    constexpr std::size_t required_size = body_offset + sizeof(std::uint32_t);
    if (runtime_record.size() < required_size) return std::nullopt;
    const auto read_float = [&runtime_record](const std::size_t offset) {
        std::uint32_t bits{};
        for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
            bits |= static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>(runtime_record[offset + byte]))
                << (byte * 8U);
        }
        return std::bit_cast<float>(bits);
    };
    return select_ncar_wheel_vertex_anchors({
        read_float(width_offset), read_float(ratio_offset),
        read_float(body_offset), model_vertex_anchor_enabled, model_vertices});
}

std::optional<NcarWheelCoreInitialization> initialize_ncar_wheel_core(
    const NcarWheelCoreInitializationInput& input) noexcept
{
    if (input.wheel_index >= 4U || input.runtime_record.size() < 0x148U)
        return std::nullopt;
    const auto read_float = [&input](const std::size_t offset) {
        std::uint32_t bits{};
        for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
            bits |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(
                input.runtime_record[offset + byte])) << (byte * 8U);
        }
        return std::bit_cast<float>(bits);
    };

    const bool front = input.wheel_index < 2U;
    const float side_sign = (input.wheel_index == 0U || input.wheel_index == 2U)
        ? -1.0F : 1.0F;
    const float width = read_float(0x58U) * 0.0254F;
    const float ratio = read_float(0x5cU) * 0.01F;
    const float longitudinal = front ? 1.0F - ratio : -ratio;
    const float lateral_setup = read_float(front ? 0x50U : 0x54U);
    const float track_scale = (front ? read_float(0xccU) : read_float(0xd0U)) *
        175.19684F;
    const float steering_x = -(read_float(front ? 0xecU : 0xf0U) *
        0.017453292F * side_sign);
    const float steering_y = -(read_float(front ? 0xf4U : 0xf8U) *
        0.017453292F * side_sign);

    const float base_height = read_float(front ? 0x60U : 0x64U) * 0.0254F;
    const float initial_value_0x44 = base_height * 2.0F;
    const float axle_setup = read_float(front ? 0x138U : 0x144U) * 0.0127F;
    const float scale_setup = read_float(front ? 0x130U : 0x13cU) * 0.001F;
    const float amount_setup = read_float(front ? 0x134U : 0x140U);
    const float accumulated_setup = amount_setup * 0.01F * scale_setup + axle_setup;
    const float mass_fraction = front ? ratio : 1.0F - ratio;
    const float body_term = input.body_mass_scale_0x1f0 * mass_fraction * 4.905F;
    const long double correction_numerator =
        static_cast<long double>(body_term) -
        static_cast<long double>(initial_value_0x44) * 0.5L * track_scale;
    const float height_correction = static_cast<float>(
        divide_with_vc98_fdiv_compatibility(correction_numerator,
            track_scale, input.fdiv_compatibility_enabled));
    const float value_0x44 = initial_value_0x44 + height_correction;
    const float value_0x48 = height_correction + base_height;
    const float axle_piecewise_input = read_float(front ? 0xd4U : 0xd8U);
    const float secondary_piecewise_input = read_float(front ? 0xdcU : 0xe0U);
    const auto scaled_piecewise_input = [
        &input](const float value, const float scale) {
        const auto inches = divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(value), 0.0254L,
            input.fdiv_compatibility_enabled);
        return static_cast<float>(inches * static_cast<long double>(scale));
    };
    const float piecewise_param1 = scaled_piecewise_input(
        axle_piecewise_input, 4.45F);
    const float piecewise_param2 = scaled_piecewise_input(
        axle_piecewise_input, 2.67F);
    const float piecewise_param3 = scaled_piecewise_input(
        secondary_piecewise_input, 4.45F);
    const float piecewise_param4 = scaled_piecewise_input(
        secondary_piecewise_input, 4.45F);
    const float positive_slope =
        (piecewise_param2 * 0.13333334F - piecewise_param1) * -1.1538461F;
    const float positive_intercept =
        (piecewise_param1 - positive_slope) * 9.84252F;
    const float nonpositive_slope =
        (piecewise_param4 * 0.13333334F - piecewise_param3) * -1.1538461F;
    const float nonpositive_intercept =
        (piecewise_param3 - nonpositive_slope) * 9.84252F;
    const auto wheel_constructor_scalar_0x4405f0 = [track_scale](
        const float caller_stack_value_0x18) {
        return (static_cast<long double>(caller_stack_value_0x18) +
            static_cast<long double>(4.44444465637207F)) *
            static_cast<long double>(track_scale) *
            static_cast<long double>(1.2499999968440534e-7F);
    };
    const long double constructor_scalar_0 = wheel_constructor_scalar_0x4405f0(
        26.66666603088379F);
    const long double constructor_scalar_1 = wheel_constructor_scalar_0x4405f0(
        71.11111450195312F);
    const float coefficient_0x9c = static_cast<float>(
        (constructor_scalar_0 * 71.11111450195312L -
         26.66666603088379L * constructor_scalar_1) *
        -1.1865233318530954e-5L);
    const float coefficient_0xa0 = static_cast<float>(
        (constructor_scalar_1 * 711.111083984375L -
         constructor_scalar_0 * 5056.79052734375L) *
        -1.1865233318530954e-5L);
    const std::array<float, 3> candidate_position{
        side_sign * std::fabs(lateral_setup) * 0.0254F * 0.5F,
        -(read_float(0x128U) * 0.0254F + value_0x44),
        width * longitudinal};
    std::array<float, 3> relative_position{};
    for (std::size_t axis = 0; axis < relative_position.size(); ++axis)
        relative_position[axis] = candidate_position[axis] -
            input.selected_model_vertex[axis];

    return NcarWheelCoreInitialization{
        input.wheel_index,
        front,
        side_sign,
        candidate_position,
        relative_position,
        input.selected_model_vertex,
        track_scale,
        accumulated_setup,
        steering_x,
        steering_y,
        value_0x44,
        value_0x48,
        0.0F,
        track_scale,
        coefficient_0x9c,
        coefficient_0xa0,
        {positive_slope, positive_intercept,
         nonpositive_slope, nonpositive_intercept}};
}

NcarFourWheelCoreInitialization initialize_ncar_four_wheel_core(
    const NcarFourWheelCoreInitializationInput& input) noexcept
{
    NcarFourWheelCoreInitialization result{};
    const auto anchors = select_ncar_wheel_vertex_anchors_from_runtime_record(
        input.runtime_record, input.model_vertices,
        input.model_vertex_anchor_enabled);
    if (!anchors.has_value()) return result;
    result.anchors = *anchors;
    for (std::size_t wheel = 0; wheel < result.wheels.size(); ++wheel) {
        const auto vertex_index = result.anchors.vertex_indices[wheel];
        if (result.anchors.sources[wheel] ==
                NcarWheelAnchorSource::model_vertex &&
            !vertex_index.has_value()) return result;
        const auto& vertex = result.anchors.selected_anchor_positions[wheel];
        result.wheels[wheel] = initialize_ncar_wheel_core({
            input.runtime_record, wheel,
            vertex,
            input.body_mass_scale_0x1f0,
            input.fdiv_compatibility_enabled});
        if (!result.wheels[wheel].has_value()) return result;
    }
    result.input_complete = true;
    return result;
}

bool apply_ncar_wheel_core_initialization(
    NcarWheelRuntimeObjectState& destination,
    const std::uint32_t wheel_object_offset,
    const NcarWheelCoreInitialization& initialization) noexcept
{
    std::array<NcarObjectFieldWrite, 17> writes{};
    std::size_t count = 0U;
    const auto write_float = [&](const std::uint32_t local_offset,
                                 const float value) {
        writes[count++] = {wheel_object_offset + local_offset, 4U,
            std::bit_cast<std::uint32_t>(value)};
    };
    for (std::size_t axis = 0; axis < 3U; ++axis) {
        write_float(0x004U + static_cast<std::uint32_t>(axis * 4U),
            initialization.position_relative_to_selected_vertex[axis]);
    }
    write_float(0x024U, initialization.suspension_piecewise_terms.positive_slope);
    write_float(0x028U, initialization.suspension_piecewise_terms.positive_intercept);
    write_float(0x02cU, initialization.suspension_piecewise_terms.nonpositive_slope);
    write_float(0x030U, initialization.suspension_piecewise_terms.nonpositive_intercept);
    write_float(0x03cU, initialization.value_0x3c);
    write_float(0x040U, initialization.value_0x40);
    write_float(0x044U, initialization.value_0x44);
    write_float(0x048U, initialization.value_0x48);
    write_float(0x04cU, initialization.value_0x4c);
    write_float(0x05cU, initialization.value_0x5c);
    write_float(0x060U, initialization.angle_0x60);
    write_float(0x064U, initialization.angle_0x64);
    write_float(0x09cU, initialization.coefficient_0x9c);
    write_float(0x0a0U, initialization.coefficient_0xa0);
    return apply_ncar_wheel_object_writes(destination, wheel_object_offset,
        std::span<const NcarObjectFieldWrite>(writes.data(), count));
}

std::optional<NcarWheelTireSetup> initialize_ncar_wheel_tire_setup(
    const std::span<const std::byte> runtime_record,
    const std::size_t wheel_index,
    const TireDefinition& tire,
    const bool fdiv_compatibility_enabled) noexcept
{
    if (wheel_index >= 4U) return std::nullopt;
    const std::size_t scale_offset = wheel_index < 2U ? 0x130U : 0x13cU;
    if (runtime_record.size() < scale_offset + sizeof(std::uint32_t))
        return std::nullopt;
    std::uint32_t scale_bits{};
    for (std::size_t byte = 0; byte < sizeof(scale_bits); ++byte) {
        scale_bits |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(
            runtime_record[scale_offset + byte])) << (byte * 8U);
    }
    const float loader_dimension_argument =
        std::bit_cast<float>(scale_bits) * 0.001F * 999.99994F;
    const float tire_response_object_0x2c = static_cast<float>(
        divide_with_vc98_fdiv_compatibility(
            loader_dimension_argument, tire.parameters[0],
            fdiv_compatibility_enabled));
    return NcarWheelTireSetup{
        wheel_index,
        loader_dimension_argument,
        tire_response_object_0x2c,
        initialize_tire_response_object(tire, tire_response_object_0x2c)};
}

NcarIntegratorWheelScalarResult advance_ncar_integrator_wheel_scalars(
    const NcarIntegratorWheelScalarInput& input) noexcept
{
    NcarIntegratorWheelScalarResult result{};
    const auto constructor_setup = input.derive_constructor_setup
        ? initialize_ncar_integrator_wheel_callback_setup(
              input.constructor_setup)
        : NcarIntegratorWheelCallbackSetup{
              input.value_0xd84, input.value_0xd90, input.angle_0xdcc};
    const auto maximum = input.value_0xde4 <=
            std::bit_cast<float>(input.value_bits_0xde8)
        ? std::bit_cast<float>(input.value_bits_0xde8)
        : input.value_0xde4;
    result.seed_values = {
        input.value_0xde4,
        input.value_0xde4,
        maximum,
        maximum};
    result.callback_values = result.seed_values;
    result.callback_0x4c_reached = true;

    if (input.selector_bits_0x4f8 != 0U) {
        result.callback_selector_nonzero = true;
        const auto speed_squared =
            static_cast<long double>(input.body_velocity_0x234.x) *
                input.body_velocity_0x234.x +
            static_cast<long double>(input.body_velocity_0x234.y) *
                input.body_velocity_0x234.y +
            static_cast<long double>(input.body_velocity_0x234.z) *
                input.body_velocity_0x234.z;
        const auto speed = std::sqrt(speed_squared);
        result.speed_magnitude = static_cast<float>(speed);
        const auto tangent = std::tan(
            static_cast<long double>(constructor_setup.angle_0xdcc) *
            static_cast<long double>(input.angle_scale_0x500));
        result.tangent_response = static_cast<float>(tangent);
        const auto target_wide =
            ((static_cast<long double>(constructor_setup.value_0xd84) -
              static_cast<long double>(constructor_setup.value_0xd90)) /
             tangent) * speed;
        result.target_response = static_cast<float>(target_wide);

        constexpr float target_cutoff = std::bit_cast<float>(0x3F638E39U);
        if (result.target_response > target_cutoff) {
            result.response_gate_passed = true;
            const auto scale_positive = input.selector_bits_0x4f8 == 1U
                ? 0.5F : 5.0F;
            constexpr float scale_negative = 5.0F;
            const auto target_delta =
                result.target_response - input.value_0xde4;
            const auto update_max = [&](const std::size_t lane,
                                        const long double candidate) {
                const auto previous = static_cast<long double>(
                    result.callback_values[lane]);
                if (!(previous >= candidate)) {
                    result.callback_values[lane] =
                        static_cast<float>(candidate);
                }
            };

            if (std::bit_cast<std::int32_t>(input.value_bits_0x244) > 0) {
                if (std::bit_cast<std::int32_t>(
                        std::bit_cast<std::uint32_t>(target_delta)) > 0) {
                    update_max(3U,
                        static_cast<long double>(target_delta) * scale_positive);
                    result.adjusted_lane = 3U;
                } else {
                    update_max(0U,
                        -static_cast<long double>(target_delta) * scale_negative);
                    result.adjusted_lane = 0U;
                }
            } else {
                const auto reversed_delta = -target_delta;
                if (!(reversed_delta > 0.0F)) {
                    update_max(1U,
                        static_cast<long double>(target_delta) * scale_negative);
                    result.adjusted_lane = 1U;
                } else {
                    update_max(2U,
                        static_cast<long double>(reversed_delta) * scale_positive);
                    result.adjusted_lane = 2U;
                }
            }
        }
    }

    constexpr std::int32_t sign_gate_bits = 0x3DCCCCCD;
    if (std::bit_cast<std::int32_t>(input.value_bits_0xde8) > sign_gate_bits) {
        result.callback_values[2] = -result.callback_values[2];
        result.callback_values[3] = -result.callback_values[3];
        result.negative_pair_gate_passed = true;
    }
    result.wheel_values_0xe0 = result.callback_values;
    return result;
}

NcarIntegratorChildControlStepResult
advance_ncar_integrator_child_control_step(
    const NcarIntegratorChildControlStepInput& input,
    const std::uint32_t car_value_0xde0_bits)
{
    NcarIntegratorChildControlStepResult result{};
    result.child = input.child;
    result.targets = input.targets;
    result.setup = setup_ncar_integrator_child_control(
        result.child,
        input.car_value_0xdec_bits,
        car_value_0xde0_bits,
        input.observation.mode_byte);
    result.controller = advance_ncar_animation_controller(
        result.child, result.targets, input.observation);
    return result;
}

bool accumulate_wheel_tire_body_force(
    NcarForceMomentAccumulator& body_accumulator,
    const WheelTireBasisUpdateResult& wheel_response,
    const PhysicsVector3& application_point_0xc8,
    const PhysicsVector3& body_center_0x5c) noexcept
{
    const auto& force = wheel_response.corrected_vector_0x100_0x108;
    const std::array<float, 3> vector{force.x, force.y, force.z};
    const std::array<float, 3> point{
        application_point_0xc8.x,
        application_point_0xc8.y,
        application_point_0xc8.z};
    const std::array<float, 3> center{
        body_center_0x5c.x,
        body_center_0x5c.y,
        body_center_0x5c.z};
    return accumulate_ncar_force_moment(body_accumulator, vector, point, center);
}

WheelSecondaryBodyForceResult accumulate_wheel_secondary_body_force(
    NcarForceMomentAccumulator& body_accumulator,
    const WheelSecondaryBodyForceInput& input) noexcept
{
    WheelSecondaryBodyForceResult result{};
    const auto scale_component = [&input](const float value) {
        auto scaled = static_cast<long double>(value) * input.factor_0x74;
        scaled *= input.factor_0x48;
        scaled *= input.factor_0x78;
        return static_cast<float>(-scaled);
    };
    result.force_vector = {
        scale_component(input.source_vector_0x10_0x18.x),
        scale_component(input.source_vector_0x10_0x18.y),
        scale_component(input.source_vector_0x10_0x18.z)};
    result.application_point = {
        static_cast<float>(static_cast<long double>(input.wheel_point_0xc8.x) *
                               input.wheel_point_weight +
                           static_cast<long double>(input.body_center_0x5c.x) *
                               input.body_center_weight),
        static_cast<float>(static_cast<long double>(input.wheel_point_0xc8.y) *
                               input.wheel_point_weight +
                           static_cast<long double>(input.body_center_0x5c.y) *
                               input.body_center_weight),
        static_cast<float>(static_cast<long double>(input.wheel_point_0xc8.z) *
                               input.wheel_point_weight +
                           static_cast<long double>(input.body_center_0x5c.z) *
                               input.body_center_weight)};
    const std::array<float, 3> vector{
        result.force_vector.x, result.force_vector.y, result.force_vector.z};
    const std::array<float, 3> point{
        result.application_point.x,
        result.application_point.y,
        result.application_point.z};
    const std::array<float, 3> center{
        input.body_center_0x5c.x,
        input.body_center_0x5c.y,
        input.body_center_0x5c.z};
    result.accumulated =
        accumulate_ncar_force_moment(body_accumulator, vector, point, center);
    return result;
}

WheelBodyForceSequenceResult accumulate_wheel_body_force_sequence(
    NcarForceMomentAccumulator& body_accumulator,
    const WheelTireBasisUpdateResult& wheel_response,
    const PhysicsVector3& application_point_0xc8,
    const PhysicsVector3& body_center_0x5c,
    const WheelSecondaryBodyForceInput& secondary_force) noexcept
{
    WheelBodyForceSequenceResult result{};
    result.tire_vector_accumulated = accumulate_wheel_tire_body_force(
        body_accumulator, wheel_response, application_point_0xc8, body_center_0x5c);
    auto secondary = secondary_force;
    secondary.wheel_point_0xc8 = application_point_0xc8;
    secondary.body_center_0x5c = body_center_0x5c;
    result.secondary_force = accumulate_wheel_secondary_body_force(
        body_accumulator, secondary);
    return result;
}

WheelConditionalTireForceResult apply_wheel_conditional_tire_force(
    NcarForceMomentAccumulator& accumulator,
    const std::int32_t car_mode_0x4f4,
    const WheelConditionalTireForceInput& input) noexcept
{
    WheelConditionalTireForceResult result{};
    if (!input.feature_gate_0x168 || car_mode_0x4f4 != 0) {
        return result;
    }
    result.gate_reached = true;

    auto traction = std::max(input.tire_scalar_0x10 - 0.5F, 0.0F) * 0.5F;
    traction = std::min(traction, 1.0F);
    result.traction_weight = std::min(
        traction, input.low_traction_flag_0x19c ? 0.1F : 1.0F);

    const auto& tire = input.corrected_tire_vector_0x100;
    const auto& forward = input.forward_axis_0x38;
    const auto forward_projection =
        (forward.x * tire.x + forward.y * tire.y) + forward.z * tire.z;
    const auto projected = forward_projection * result.traction_weight;
    result.force_vector = {
        projected * forward.x * input.response_scale,
        projected * forward.y * input.response_scale,
        projected * forward.z * input.response_scale};

    const auto& velocity = input.body_velocity_0x234;
    result.velocity_squared =
        (velocity.x * velocity.x + velocity.z * velocity.z) +
        velocity.y * velocity.y;
    if (result.velocity_squared > 0.1F) {
        const auto inverse_speed = 1.0F / std::sqrt(result.velocity_squared);
        const auto forward_force_projection =
            (forward.x * result.force_vector.x +
             forward.y * result.force_vector.y) +
            forward.z * result.force_vector.z;
        const auto damping = -std::fabs(forward_force_projection) * 2.5F;
        result.force_vector.x += velocity.x * inverse_speed * damping;
        result.force_vector.y += velocity.y * inverse_speed * damping;
        result.force_vector.z += velocity.z * inverse_speed * damping;
    }

    const std::array<float, 3> force{
        result.force_vector.x, result.force_vector.y, result.force_vector.z};
    const std::array<float, 3> center{
        input.body_center_0x5c.x, input.body_center_0x5c.y,
        input.body_center_0x5c.z};
    result.accumulated = accumulate_ncar_force_moment(
        accumulator, force, center, center);
    return result;
}

NcarIntegratorVirtualRecordCallbackResult
apply_ncar_integrator_virtual_record_callback(
    NcarPostBodyState& state,
    const NcarIntegratorVirtualRecordCallbackInput& input) noexcept
{
    NcarIntegratorVirtualRecordCallbackResult result{};
    result.callback_reached = true;
    result.global_gate_passed = input.global_update_gate_0x425870;
    if (!result.global_gate_passed) {
        return result;
    }

    const auto& force = input.record.normalized_first_vector;
    result.force_magnitude_squared =
        force.z * force.z + force.y * force.y + force.x * force.x;
    result.impact_threshold_passed =
        result.force_magnitude_squared > 550069.4F;
    if (!result.impact_threshold_passed) {
        return result;
    }

    state.timeout_pending_0x48c = 1U;
    state.timeout_start_0x488 = input.current_time_0x425790;
    result.timeout_state_written = true;
    result.timeout_start_time_written = input.current_time_0x425790;

    const PhysicsVector3 body_local_offset{
        input.body_local_offset_0x28[0], input.body_local_offset_0x28[1],
        input.body_local_offset_0x28[2]};
    const PhysicsVector3 body_center{
        input.body_center_0x5c[0], input.body_center_0x5c[1],
        input.body_center_0x5c[2]};
    const auto world_reference = transform_body_local_point(
        {body_local_offset, input.body_orientation_matrix_0x38, body_center});
    const PhysicsVector3 relative_position{
        input.record.second_vector.x - world_reference.x,
        input.record.second_vector.y - world_reference.y,
        input.record.second_vector.z - world_reference.z};
    result.event_local_position = transform_body_angular_accumulator(
        relative_position, input.body_orientation_matrix_0x38);
    result.event_local_force = transform_body_angular_accumulator(
        input.record.normalized_first_vector,
        input.body_orientation_matrix_0x38);

    const auto ftol = [](const long double value) noexcept {
        const auto rounded = std::nearbyint(value);
        if (!std::isfinite(rounded) ||
            rounded < static_cast<long double>(
                          std::numeric_limits<std::int32_t>::min()) ||
            rounded > static_cast<long double>(
                          std::numeric_limits<std::int32_t>::max())) {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(rounded);
    };
    const auto packed_position = [&](const float value) noexcept {
        return static_cast<std::uint8_t>(
            static_cast<std::uint32_t>(ftol(
                divide_with_vc98_fdiv_compatibility(
                    static_cast<long double>(value), 10.0L,
                    input.fdiv_compatibility_enabled))) >> 8U);
    };
    const auto packed_force = [&](const float value) noexcept {
        return static_cast<std::uint16_t>(ftol(
            divide_with_vc98_fdiv_compatibility(
                static_cast<long double>(value), 30000.0L,
                input.fdiv_compatibility_enabled)));
    };
    result.event_payload[0] = packed_position(result.event_local_position.x);
    result.event_payload[1] = packed_position(result.event_local_position.y);
    result.event_payload[2] = packed_position(result.event_local_position.z);
    for (std::size_t axis = 0; axis < 3U; ++axis) {
        const auto force_component = axis == 0U
            ? result.event_local_force.x
            : axis == 1U ? result.event_local_force.y
                         : result.event_local_force.z;
        const auto packed = packed_force(force_component);
        const auto offset = 3U + axis * 2U;
        result.event_payload[offset] = static_cast<std::uint8_t>(packed);
        result.event_payload[offset + 1U] =
            static_cast<std::uint8_t>(packed >> 8U);
    }
    result.event_payload[13] = static_cast<std::uint8_t>(
        input.vehicle_ordinal_0x510);
    result.event_payload_bytes_resolved = true;
    result.secondary_virtual_call_reached = true;
    result.mesh_response = apply_ncar_integrator_mesh_response(
        state, input, result.event_local_force, result.event_local_position);
    return result;
}

NcarMeshBoundsMaskResult apply_ncar_mesh_bounds_mask(
    const NcarMeshBoundsMaskInput& input) noexcept
{
    constexpr auto mask_coordinate_scale = std::bit_cast<float>(0x417FD70AU);
    const auto ftol = [](const long double value) noexcept {
        const auto rounded = std::nearbyint(value);
        if (!std::isfinite(rounded) ||
            rounded < static_cast<long double>(
                          std::numeric_limits<std::int32_t>::min()) ||
            rounded > static_cast<long double>(
                          std::numeric_limits<std::int32_t>::max())) {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(rounded);
    };
    const auto scaled_index = [&](const float value) noexcept {
        return ftol(static_cast<long double>(value) *
                    static_cast<long double>(mask_coordinate_scale));
    };

    NcarMeshBoundsMaskResult result{};
    result.row_begin_index = scaled_index(input.row_begin);
    result.row_end_index = scaled_index(input.row_end);
    result.column_begin_index = scaled_index(input.column_begin);
    result.column_end_index = scaled_index(input.column_end);

    const auto column_count = static_cast<std::int64_t>(
        result.column_end_index) - result.column_begin_index;
    if (column_count >= 32) {
        result.column_bits = 0xffffU;
    } else if (column_count > 0) {
        for (auto column = static_cast<std::int64_t>(
                 result.column_begin_index);
             column < result.column_end_index; ++column) {
            const auto shift = static_cast<unsigned>(
                static_cast<std::uint64_t>(column) & 31U);
            if (shift < 16U) {
                result.column_bits = static_cast<std::uint16_t>(
                    result.column_bits | (std::uint16_t{1} << shift));
            }
        }
    }

    const auto row_count = static_cast<std::int64_t>(result.row_end_index) -
        result.row_begin_index;
    if (row_count <= 0) {
        return result;
    }
    const auto bounded_begin = std::max<std::int64_t>(
        result.row_begin_index, 0);
    const auto bounded_end = std::min<std::int64_t>(
        result.row_end_index,
        static_cast<std::int64_t>(input.row_words.size()));
    for (auto row = bounded_begin; row < bounded_end; ++row) {
        input.row_words[static_cast<std::size_t>(row)] =
            static_cast<std::uint16_t>(
                input.row_words[static_cast<std::size_t>(row)] |
                result.column_bits);
        ++result.rows_written;
    }
    result.rows_out_of_range = static_cast<std::uint64_t>(row_count) -
        result.rows_written;
    return result;
}

NcarIntegratorMeshResponseResult apply_ncar_integrator_mesh_response(
    NcarPostBodyState& state,
    const NcarIntegratorVirtualRecordCallbackInput& input,
    const PhysicsVector3& local_force,
    const PhysicsVector3& local_position) noexcept
{
    NcarIntegratorMeshResponseResult result{};
    result.handler_reached = true;
    state.timeout_pending_0x48c = 1U;
    result.timeout_state_written = true;
    result.input_complete = input.mesh_vertex_buffer_resolved &&
        input.mesh_vertex_index_range_resolved;
    if (!input.mesh_vertex_buffer_resolved ||
        !input.mesh_vertex_index_range_resolved) {
        return result;
    }

    constexpr auto force_squared_offset = 741.6666F;
    constexpr auto force_to_damage_scale = 0.0026966294F;
    constexpr auto maximum_damage_scale = 40.0F;
    constexpr auto vertex_displacement_step = 0.016F;
    constexpr auto near_vertex_distance_squared = 0.36F;
    constexpr auto focus_offset = 0.48000002F;

    const auto force_magnitude = std::sqrt(
        (static_cast<long double>(local_force.y) * local_force.y +
         static_cast<long double>(local_force.z) * local_force.z) +
        static_cast<long double>(local_force.x) * local_force.x);
    const auto force_excess = static_cast<float>(
        force_magnitude - static_cast<long double>(force_squared_offset));
    const auto normalized_component = [&](const float component) noexcept {
        return static_cast<float>(divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(component), force_magnitude,
            input.fdiv_compatibility_enabled));
    };
    const PhysicsVector3 force_direction{
        normalized_component(local_force.x),
        normalized_component(local_force.y),
        normalized_component(local_force.z)};
    auto damage_scale = force_excess * force_to_damage_scale;
    if (damage_scale > maximum_damage_scale) {
        damage_scale = maximum_damage_scale;
    }
    result.displacement_scale = damage_scale * vertex_displacement_step;
    const auto displacement_component = [&](const float component) noexcept {
        return component * damage_scale * vertex_displacement_step;
    };

    const auto query_x = local_position.x * 1.3F;
    const auto query_y_minus_one = local_position.y - 1.0F;
    const auto query_y_scaled = query_y_minus_one * 1.3F;
    const auto query_y = query_y_scaled + 1.0F;
    const auto query_z = local_position.z * 1.3F;
    const PhysicsVector3 query_position{
        query_x, query_y, query_z};
    auto nearest_candidate = local_force;
    auto nearest_distance_squared = 1.0e6F;
    for (std::size_t index = 0; index < input.mesh_vertices.size(); ++index) {
        const auto& vertex = input.mesh_vertices[index];
        const auto dx = vertex[0] - query_position.x;
        const auto dy = vertex[1] - query_position.y;
        const auto dz = vertex[2] - query_position.z;
        const auto distance_squared = dx * dx + dz * dz + dy * dy;
        if (distance_squared < nearest_distance_squared) {
            nearest_distance_squared = distance_squared;
            nearest_candidate = {vertex[0], vertex[1], vertex[2]};
            result.nearest_vertex_index = index;
            result.nearest_vertex_found = true;
        }
    }
    result.mesh_focus_center = {
        nearest_candidate.x - force_direction.x * focus_offset,
        nearest_candidate.y - force_direction.y * focus_offset,
        nearest_candidate.z - force_direction.z * focus_offset};

    constexpr std::array<std::array<float, 2>, 2> vertical_bands{{
        {{0.0F, 0.75F}}, {{0.75F, 1.0F}}}};
    for (std::size_t pass = 0; pass < vertical_bands.size(); ++pass) {
        auto lower_u = 0.0F;
        auto upper_u = 1.0F;
        auto lower_v = 0.0F;
        auto upper_v = 1.0F;
        for (std::size_t index = 0; index < input.mesh_vertices.size(); ++index) {
            auto& vertex = input.mesh_vertices[index];
            const auto dx = vertex[0] - result.mesh_focus_center.x;
            const auto dy = vertex[1] - result.mesh_focus_center.y;
            const auto dz = vertex[2] - result.mesh_focus_center.z;
            const auto distance_squared = dx * dx + dz * dz + dy * dy;
            if (distance_squared >= near_vertex_distance_squared) {
                continue;
            }

            vertex[0] += displacement_component(force_direction.x);
            vertex[1] += displacement_component(force_direction.y);
            vertex[2] += displacement_component(force_direction.z);
            ++result.vertex_displacement_stores;

            const auto uv_x = vertex[6];
            const auto uv_y = vertex[7];
            const auto vertex_index = static_cast<std::int64_t>(index);
            if (vertex_index < input.mesh_vertex_index_begin ||
                vertex_index >= input.mesh_vertex_index_end ||
                uv_y < vertical_bands[pass][0] ||
                uv_y >= vertical_bands[pass][1]) {
                continue;
            }
            upper_u = std::min(upper_u, uv_x - 0.05F);
            upper_v = std::min(upper_v, uv_y - 0.05F);
            lower_u = std::max(lower_u, uv_x + 0.05F);
            lower_v = std::max(lower_v, uv_y + 0.05F);
        }

        auto mask_rows = std::span<std::uint16_t>{state.timeout_values_0x48e};
        result.bounds_mask_updates[pass] = apply_ncar_mesh_bounds_mask({
            mask_rows, upper_v, lower_v, upper_u, lower_u});
        if (result.bounds_mask_updates[pass].rows_out_of_range != 0U) {
            result.input_complete = false;
        }
    }

    result.main_descriptor_refresh_reached = true;
    state.integrator_mesh_descriptor_flag_0x15 = 0U;
    if (input.mesh_descriptor_pointer_0x10) {
        state.integrator_mesh_descriptor_0x10 =
            *input.mesh_descriptor_pointer_0x10;
    } else {
        result.input_complete = false;
    }

    for (std::size_t group_index = 0;
         group_index < input.mesh_groups.size(); ++group_index) {
        const auto& group = input.mesh_groups[group_index];
        result.group_descriptor_refresh_reached[group_index] = true;
        state.resource_flag_0x15[group_index] = 0U;
        if (group.destination_vertex_buffer_resolved &&
            (group.destination_vertices.empty() ||
             group.source_vertex_index_buffer_resolved)) {
            const auto count = std::min(group.destination_vertices.size(),
                                        group.source_vertex_indices.size());
            for (std::size_t vertex_index = 0; vertex_index < count;
                 ++vertex_index) {
                const auto source_index = static_cast<std::size_t>(
                    group.source_vertex_indices[vertex_index]);
                if (source_index >= input.mesh_vertices.size()) {
                    ++result.group_vertices_unprocessed[group_index];
                    continue;
                }
                auto& destination = group.destination_vertices[vertex_index];
                const auto& source = input.mesh_vertices[source_index];
                destination[0] = source[0];
                destination[1] = source[1];
                destination[2] = source[2];
                ++result.group_vertices_copied[group_index];
            }
            result.group_vertices_unprocessed[group_index] +=
                group.destination_vertices.size() - count;
            if (result.group_vertices_unprocessed[group_index] != 0U) {
                result.input_complete = false;
            }
        } else {
            result.group_vertices_unprocessed[group_index] =
                group.destination_vertices.size();
            result.input_complete = false;
        }

        if (group.descriptor_pointer_0x10) {
            state.resource_descriptor_0x10[group_index] =
                *group.descriptor_pointer_0x10;
        } else {
            result.input_complete = false;
        }
    }
    return result;
}

NcarPostBodyResult advance_ncar_post_body_tail(
    NcarPostBodyState& state,
    const NcarPostBodyInput& input) noexcept
{
    NcarPostBodyResult result{};
    auto transformed_vector = input.transformed_vector_0x382;
    if (input.derive_vector_0x382) {
        for (std::size_t row = 0; row < transformed_vector.size(); ++row) {
            const auto value =
                static_cast<long double>(input.orientation_matrix_0x38[row]) *
                    input.source_vector_0xd88[0] +
                static_cast<long double>(input.orientation_matrix_0x38[row + 3U]) *
                    input.source_vector_0xd88[1] +
                static_cast<long double>(input.orientation_matrix_0x38[row + 6U]) *
                    input.source_vector_0xd88[2];
            transformed_vector[row] = static_cast<float>(value);
        }
    }
    state.cleared_vector_0x37f = {};
    for (std::size_t axis = 0; axis < state.vector_0x382.size(); ++axis) {
        state.vector_0x382[axis] = static_cast<float>(
            static_cast<long double>(transformed_vector[axis]) -
            static_cast<long double>(input.body_axis_0x14[axis]) *
                input.half_offset_scale_0x385 * 0.5L);
    }

    const std::array<long double, 3> normalized_axis{
        -static_cast<long double>(input.body_axis_0x14[0]),
        0.0L,
        static_cast<long double>(input.body_axis_0x14[2])};
    const auto axis_length_squared = normalized_axis[0] * normalized_axis[0] +
        normalized_axis[1] * normalized_axis[1] +
        normalized_axis[2] * normalized_axis[2];
    if (axis_length_squared > 0.001L) {
        const auto inverse_length = 1.0L / std::sqrt(axis_length_squared);
        const std::array<long double, 3> axis{
            normalized_axis[0] * inverse_length,
            normalized_axis[1] * inverse_length,
            normalized_axis[2] * inverse_length};
        const auto scale = 0.0101936795L;
        const std::array<long double, 3> increments{
            (static_cast<long double>(input.angular_velocity_0xa2[0]) * axis[2] +
             static_cast<long double>(input.angular_velocity_0xa2[2]) * axis[0]) * scale,
            (static_cast<long double>(input.angular_velocity_0xa2[2]) * axis[2] -
             static_cast<long double>(input.angular_velocity_0xa2[0]) * axis[0]) * scale,
            (static_cast<long double>(input.angular_velocity_0xa2[0]) * input.reference_axis_0x44[0] +
             static_cast<long double>(input.angular_velocity_0xa2[1]) * input.reference_axis_0x44[1] +
             static_cast<long double>(input.angular_velocity_0xa2[2]) * input.reference_axis_0x44[2]) * scale};
        for (std::size_t axis_index = 0; axis_index < increments.size(); ++axis_index) {
            state.angular_accumulator_0x3a9[axis_index] = static_cast<float>(
                static_cast<long double>(state.angular_accumulator_0x3a9[axis_index]) * 0.9L +
                increments[axis_index]);
        }
        result.angular_accumulators_updated = true;
    }

    const auto contact_count = std::accumulate(
        input.wheel_contact_bytes.begin(), input.wheel_contact_bytes.end(), 0U);
    if (contact_count == 4U &&
        ncar_integrator_wear_gate_0x433a00(input.wear_value_bits) == 0U) {
        const auto x = static_cast<long double>(input.linear_velocity_0x234[0]);
        const auto y = static_cast<long double>(input.linear_velocity_0x234[1]);
        const auto z = static_cast<long double>(input.linear_velocity_0x234[2]);
        auto speed = std::sqrt(x * x + y * y + z * z);
        if (speed <= state.maximum_speed_0x38c) {
            speed = state.maximum_speed_0x38c;
        }
        state.maximum_speed_0x38c = static_cast<float>(speed);
        result.maximum_speed_store_reached = true;
    }

    if (!input.global_update_gate_0x425870 && state.timeout_pending_0x48c != 0U &&
        static_cast<long double>(input.current_time_0x425790) >
            static_cast<long double>(state.timeout_start_0x488) + 2.0L) {
        result.timeout_callback_0x433920_reached = true;
        state.timeout_auxiliary_0x48d = 0U;
        state.timeout_values_0x48e.fill(0U);
        state.timeout_pending_0x48c = 0U;
        result.wheel_damage_flags_cleared = true;
        result.backend_virtual_0x478_reached = true;
        const auto resource_reset = reset_ncar_resource_groups(
            input.timeout_resource_groups, state.resource_flag_0x15);
        result.resource_groups_visited = resource_reset.groups_visited;
        result.resource_records_copied = resource_reset.records_copied;
        result.resource_records_unprocessed =
            resource_reset.records_unprocessed;
        result.resource_group_inputs_complete = resource_reset.inputs_complete;
    }
    result.integrator_callback_0x508a04_slot_0x38_reached = true;
    result.integrator_callback_vehicle_ordinal_0x510 =
        input.vehicle_ordinal_0x510;
    return result;
}

std::uint8_t ncar_integrator_wear_gate_0x433a00(
    const std::array<std::uint32_t, 4>& wear_value_bits) noexcept
{
    std::uint8_t result = 0U;
    for (std::size_t index = 0; index < wear_value_bits.size(); ++index) {
        if (std::bit_cast<std::int32_t>(wear_value_bits[index]) > 0x3fb9999a) {
            result = static_cast<std::uint8_t>(result | (1U << index));
        }
    }
    return result;
}

NcarIntegratorWheelPairDifferentialResult
apply_ncar_integrator_wheel_pair_differentials(
    const NcarIntegratorWheelPairDifferentialInput& input) noexcept
{
    const auto first = static_cast<float>(
        (static_cast<long double>(input.first_value_0x63c) -
         input.first_reference_0x7e0) * input.first_scale_0xdd4);
    const auto second = static_cast<float>(
        (static_cast<long double>(input.second_value_0x984) -
         input.second_reference_0xb28) * input.second_scale_0xdd8);
    const auto first_applied = input.contact_bytes[0] != 0U ||
        input.contact_bytes[1] != 0U ? first : 0.0F;
    const auto second_applied = input.contact_bytes[2] != 0U ||
        input.contact_bytes[3] != 0U ? second : 0.0F;
    return {
        -first_applied,
        first_applied,
        -second_applied,
        second_applied};
}

NcarIntegratorCommonForceResult apply_ncar_integrator_common_force(
    NcarForceMomentAccumulator& accumulator,
    const NcarIntegratorCommonForceInput& input) noexcept
{
    NcarIntegratorCommonForceResult result{};
    result.contact_byte_sum = std::accumulate(
        input.contact_bytes.begin(), input.contact_bytes.end(), 0U);
    result.high_contact_branch = result.contact_byte_sum >= 2U;

    const auto dot = [](const PhysicsVector3& left, const PhysicsVector3& right) {
        return (left.x * right.x + left.y * right.y) + left.z * right.z;
    };
    if (!result.high_contact_branch) {
        const auto x_projection = dot(input.reference_axis_0x44,
                                      input.velocity_delta_0x234_0xdfc);
        const auto y_projection = dot(input.body_axis_0x50,
                                      input.velocity_delta_0x234_0xdfc);
        const auto z_projection = dot(input.force_axis_0x38,
                                      input.velocity_delta_0x234_0xdfc);
        const auto x_resistance = x_projection * input.resistance_scale_0x150;
        const auto y_resistance = y_projection * input.resistance_scale_0x14e;
        const auto z_resistance = z_projection * input.resistance_scale_0x14f;
        const auto magnitude = std::sqrt(
            (x_resistance * x_resistance + y_resistance * y_resistance) +
            z_resistance * z_resistance);
        result.local_force = {
            -input.velocity_delta_0x234_0xdfc.x * magnitude,
            -input.velocity_delta_0x234_0xdfc.y * magnitude,
            -input.velocity_delta_0x234_0xdfc.z * magnitude};
        result.force_vector = result.local_force;
    } else {
        const auto quadratic_resistance = [](const float component, const float scale) {
            return -std::fabs(component) * component * scale;
        };
        result.local_force = {
            quadratic_resistance(input.transformed_velocity_delta_0xdf0.x,
                                 input.ground_scale_0x53c),
            quadratic_resistance(input.transformed_velocity_delta_0xdf0.y,
                                 input.ground_scale_0x540),
            quadratic_resistance(input.transformed_velocity_delta_0xdf0.z,
                                 input.ground_scale_0x538)};
        const auto& m = input.transform_0x38_to_0x58;
        result.force_vector = {
            (m[0] * result.local_force.x + m[3] * result.local_force.y) +
                m[6] * result.local_force.z,
            (m[1] * result.local_force.x + m[4] * result.local_force.y) +
                m[7] * result.local_force.z,
            (m[2] * result.local_force.x + m[5] * result.local_force.y) +
                m[8] * result.local_force.z};
    }

    const std::array<float, 3> force{
        result.force_vector.x, result.force_vector.y, result.force_vector.z};
    const std::array<float, 3> point{
        input.application_point.x, input.application_point.y,
        input.application_point.z};
    const std::array<float, 3> center{
        input.body_center.x, input.body_center.y, input.body_center.z};
    result.accumulated = accumulate_ncar_force_moment(
        accumulator, force, point, center);
    return result;
}

NcarIntegratorVelocityForceResult apply_ncar_integrator_velocity_forces(
    NcarForceMomentAccumulator& accumulator,
    const NcarIntegratorVelocityForceInput& input) noexcept
{
    NcarIntegratorVelocityForceResult result{};
    const auto dot = [](const PhysicsVector3& left, const PhysicsVector3& right) {
        return (left.x * right.x + left.y * right.y) + left.z * right.z;
    };
    const auto& velocity = input.velocity_vector;
    result.speed = std::sqrt(dot(velocity, velocity));
    result.projection = dot(input.projection_axis_0x50, velocity);
    const auto projection_squared = result.projection * result.projection;
    constexpr float energy_scale = -0.101936795F;

    for (std::size_t index = 0; index < result.force_vectors.size(); ++index) {
        const auto quadratic = projection_squared * input.quadratic_scale[index];
        const auto damping = -(input.damping_scale[index] * result.speed);
        result.force_vectors[index] = {
            velocity.x * damping + input.force_axis_0x44.x * quadratic,
            velocity.y * damping + input.force_axis_0x44.y * quadratic,
            velocity.z * damping + input.force_axis_0x44.z * quadratic};
        const auto& force = result.force_vectors[index];
        const std::array<float, 3> force_components{force.x, force.y, force.z};
        const auto& point = input.application_points[index];
        const std::array<float, 3> point_components{point.x, point.y, point.z};
        const std::array<float, 3> center_components{
            input.body_center.x, input.body_center.y, input.body_center.z};
        result.accumulated[index] = accumulate_ncar_force_moment(
            accumulator, force_components, point_components, center_components);
        result.per_step_energy_0xeb4 +=
            input.mass_0x200 * force.y * energy_scale;
    }
    return result;
}

NcarAttachedObjectForceResult apply_ncar_attached_object_force(
    NcarForceMomentAccumulator& accumulator,
    const NcarAttachedObjectForceInput& input) noexcept
{
    NcarAttachedObjectForceResult result{};
    const auto dot = [](const PhysicsVector3& left, const PhysicsVector3& right) {
        return (left.x * right.x + left.y * right.y) + left.z * right.z;
    };
    const auto& velocity = input.transformed_relative_velocity;
    result.speed = std::sqrt(dot(velocity, velocity));
    const auto denominator = std::max(result.speed, 1.0F);
    const PhysicsVector3 normalized_velocity{
        velocity.x / denominator, velocity.y / denominator,
        velocity.z / denominator};
    const auto& selected_basis = input.select_basis_0x44
        ? input.object_basis_0x44
        : input.object_basis_0x38;
    result.normalized_projection = dot(selected_basis, normalized_velocity);

    const auto projection_error = input.target_projection_0x38 -
        result.normalized_projection;
    result.slip_factor = std::clamp(
        projection_error * 3.8197186F, -1.0F, 1.0F) * 1.6F;
    if (dot(input.body_axis_0x50, velocity) < 0.0F) {
        result.slip_factor = 0.0F;
    }
    result.resistance_factor = std::min(
        projection_error * projection_error * 0.13131225F, 0.05F) + 0.006F;
    const auto scale = input.coefficient_0x04 * input.coefficient_0x08 * 0.2F *
        (result.speed * result.speed) * 0.645F;
    result.force = {
        (result.slip_factor * selected_basis.x -
         input.body_axis_0x50.x * result.resistance_factor) * scale,
        (result.slip_factor * selected_basis.y -
         input.body_axis_0x50.y * result.resistance_factor) * scale,
        (result.slip_factor * selected_basis.z -
         input.body_axis_0x50.z * result.resistance_factor) * scale};

    const std::array<float, 3> force{result.force.x, result.force.y, result.force.z};
    const std::array<float, 3> point{
        input.application_point_0x28.x, input.application_point_0x28.y,
        input.application_point_0x28.z};
    const std::array<float, 3> center{
        input.body_center.x, input.body_center.y, input.body_center.z};
    result.applied = accumulate_ncar_force_moment(accumulator, force, point, center);
    return result;
}

NcarIntegratorModeForceResult apply_ncar_integrator_mode_force(
    NcarForceMomentAccumulator& accumulator,
    const NcarIntegratorModeForceInput& input) noexcept
{
    NcarIntegratorModeForceResult result{};
    if (input.mode_0x4f4 != 0 && input.mode_0x4f4 != 1) {
        return result;
    }
    result.supported_mode = true;

    const auto dot = [](const PhysicsVector3& left, const PhysicsVector3& right) {
        return (left.x * right.x + left.y * right.y) + left.z * right.z;
    };
    const auto squared_length = [](const PhysicsVector3& value) {
        return (value.x * value.x + value.y * value.y) + value.z * value.z;
    };
    const auto angular_projection = dot(
        input.angular_velocity_0x240, input.body_axis_0x44);
    const auto velocity_squared = squared_length(input.linear_velocity_0x234);
    auto speed_vector = input.linear_velocity_0x234;
    auto mode_scale = input.mode_0x4f4 == 0 ? 1.0F : 2.0F;
    const auto apply_wear_scale = [&input, &mode_scale]() {
        for (std::size_t index = 0; index < input.wear_value_bits.size(); ++index) {
            if (std::bit_cast<std::int32_t>(input.wear_value_bits[index]) >
                0x3dcccccd) {
                mode_scale *= index < 2U ? 0.9F : 1.3F;
            }
        }
    };
    if (input.mode_0x4f4 == 0) {
        apply_wear_scale();
        auto response_scale = std::fabs(angular_projection) * 2.5F;
        if (response_scale <= 0.4F) {
            response_scale = 0.4F;
        }
        if (response_scale >= 1.0F) {
            response_scale = 1.0F;
        }
        if (velocity_squared < 177.77777F) {
            const auto speed_factor = velocity_squared * 0.005625F;
            response_scale *= speed_factor;
            mode_scale *= speed_factor;
        }
        if (velocity_squared > 1.0F) {
            const auto root = std::sqrt(velocity_squared);
            const auto reciprocal = static_cast<float>(
                divide_with_vc98_fdiv_compatibility(
                    1.0L, root, input.fdiv_compatibility_enabled));
            speed_vector = {
                speed_vector.x * reciprocal,
                speed_vector.y * reciprocal,
                speed_vector.z * reciprocal};
        }
        auto force_magnitude = input.force_scale_0xdcc *
            input.force_scale_0x500 * mode_scale * 0.9F;
        response_scale *= 0.99F;
        const auto forward_projection = dot(input.body_axis_0x50, speed_vector);
        if (forward_projection < 0.98F) {
            mode_scale = force_magnitude * 0.5F;
            force_magnitude = angular_projection * 0.2F;
        } else {
            mode_scale = force_magnitude;
            force_magnitude = angular_projection;
        }
        const auto scalar = mode_scale * 1000.0F + force_magnitude * -140.0F;
        result.force_vector = {
            input.force_axis_0x38.x * response_scale * scalar,
            input.force_axis_0x38.y * response_scale * scalar,
            input.force_axis_0x38.z * response_scale * scalar};
    } else {
        if (velocity_squared < 1.0F) {
            mode_scale = velocity_squared * 2.0F;
        }
        apply_wear_scale();
        const auto angular_magnitude = std::fabs(angular_projection) * 2.5F;
        const auto tire_response = std::min(
            1.0F, std::max(0.2F, angular_magnitude));
        auto force_magnitude = angular_projection * -0.05F +
            input.force_scale_0xdcc * input.force_scale_0x500 * mode_scale * 0.1F;
        force_magnitude *= 1000.0F;
        result.force_vector = {
            input.force_axis_0x38.x * tire_response * force_magnitude,
            input.force_axis_0x38.y * tire_response * force_magnitude,
            input.force_axis_0x38.z * tire_response * force_magnitude};
    }

    const auto contact_count = std::accumulate(
        input.contact_bytes.begin(), input.contact_bytes.end(), 0U);
    result.contact_gate_passed = contact_count > 2U &&
        input.leading_wheel_damage_bytes[0] == 0U &&
        input.leading_wheel_damage_bytes[1] == 0U;
    if (!result.contact_gate_passed) {
        return result;
    }
    const auto forward_projection = dot(
        input.body_axis_0x50, input.mode_0x4f4 == 0
            ? speed_vector : input.linear_velocity_0x234);
    result.forward_gate_passed = forward_projection > 0.1F;
    if (!result.forward_gate_passed) {
        return result;
    }

    const std::array<float, 3> force{
        result.force_vector.x, result.force_vector.y, result.force_vector.z};
    const std::array<float, 3> point{
        input.application_point.x,
        input.application_point.y,
        input.application_point.z};
    const std::array<float, 3> center{
        input.body_center.x, input.body_center.y, input.body_center.z};
    result.accumulated = accumulate_ncar_force_moment(
        accumulator, force, point, center);
    return result;
}

WheelCollisionImpulseApplicationResult apply_wheel_collision_impulse(
    NcarPointImpulseAccumulator& accumulator,
    std::uint8_t& wheel_damage_flag_0x20,
    const WheelCollisionImpulseInput& input) noexcept
{
    WheelCollisionImpulseApplicationResult result{};
    result.response = prepare_wheel_collision_impulse(input);
    if (!result.response.applied) {
        return result;
    }

    if (result.response.set_wheel_damage_flag) {
        wheel_damage_flag_0x20 = 1U;
        result.damage_flag_written = true;
    }

    const std::array<float, 3> impulse{
        result.response.impulse.x,
        result.response.impulse.y,
        result.response.impulse.z};
    const std::array<float, 3> point{
        input.contact_point.x,
        input.contact_point.y,
        input.contact_point.z};
    const std::array<float, 3> center{
        input.body_center.x,
        input.body_center.y,
        input.body_center.z};
    accumulate_ncar_point_impulse(
        accumulator, impulse, point, center, result.response.duration);
    result.point_impulse_accumulated = true;
    return result;
}

FourWheelCollisionImpulseResult apply_four_wheel_collision_impulses(
    NcarPointImpulseAccumulator& accumulator,
    const std::array<std::uint8_t, 4>& wheel_damage_flags_0x20,
    const std::array<WheelCollisionImpulseInput, 4>& inputs) noexcept
{
    FourWheelCollisionImpulseResult result{};
    result.wheel_damage_flags = wheel_damage_flags_0x20;
    constexpr std::size_t wheel_stride_bytes = 0x1a4U;
    for (std::size_t wheel = 0; wheel < inputs.size(); ++wheel) {
        result.wheel_subobject_offsets[wheel] = wheel * wheel_stride_bytes;
        result.wheel_results[wheel] = apply_wheel_collision_impulse(
            accumulator, result.wheel_damage_flags[wheel], inputs[wheel]);
        ++result.wheel_calls;
    }
    return result;
}

FourWheelBodyStepResult advance_four_wheel_body_step(
    const FourWheelBodyStepInput& input)
{
    FourWheelBodyStepResult result{};
    result.integrator_bookkeeping_state = input.integrator_bookkeeping_state;
    result.wheel_animation_states = input.wheel_animation_states;
    result.vehicle_animation_channel_0x18 =
        input.vehicle_animation_channel_0x18;
    result.vehicle_animation_channel_0x5c =
        input.vehicle_animation_channel_0x5c;
    result.body_tail_state = input.body_tail_state;
    result.post_body_state = input.post_body_state;
    result.point_impulse_state = input.point_impulse_state;
    result.force_moment_state = input.force_moment_state;
    result.wheel_tire_dynamics_states = input.wheel_tire_dynamics_states;
    result.wheel_tire_accumulator_states = input.wheel_tire_accumulator_states;
    result.wheel_contact_states = input.wheel_contact_states;
    result.energy_accumulator_0xe40 = input.energy_accumulator_0xe40;
    auto kinematics_at_step_start = input.kinematics;
    result.integrator_prefix = apply_ncar_integrator_prefix(
        kinematics_at_step_start,
        result.force_moment_state,
        input.integrator_prefix_input,
        input.mode_force_input.contact_bytes);
    result.integrator_timer_state = input.integrator_timer_state;
    result.integrator_timer = advance_ncar_integrator_timer(
        result.integrator_timer_state,
        input.integrator_timer_input);
    result.integrator_early_bounds = plan_ncar_integrator_early_bounds(
        kinematics_at_step_start.position,
        input.post_body_input.vehicle_ordinal_0x510);
    result.integrator_curve_gate_state = input.integrator_curve_gate_state;
    result.integrator_curve_gate = advance_ncar_integrator_curve_gate(
        result.integrator_curve_gate_state,
        input.integrator_curve_gate_input);
    result.integrator_child_control = advance_ncar_integrator_child_control_step(
        input.integrator_child_control_input,
        std::bit_cast<std::uint32_t>(
            result.integrator_curve_gate_state.value_0xde0));
    result.integrator_signal_state = input.integrator_signal_state;
    result.integrator_signal_state.value_bits_0xc7c =
        input.integrator_curve_gate_input.parent_value_bits_0xc7c;
    result.integrator_signal = advance_ncar_integrator_signal(
        result.integrator_signal_state,
        input.integrator_signal_input,
        result.integrator_curve_gate_state.parent_flag_0xc3c);
    auto wheel_scalar_input = input.integrator_wheel_scalar_input;
    wheel_scalar_input.body_velocity_0x234 =
        kinematics_at_step_start.linear_velocity;
    wheel_scalar_input.value_bits_0x244 = std::bit_cast<std::uint32_t>(
        kinematics_at_step_start.angular_velocity.y);
    result.integrator_wheel_scalars = advance_ncar_integrator_wheel_scalars(
        wheel_scalar_input);
    result.input_complete = result.integrator_timer.input_complete;
    result.embedded_records.input_complete =
        input.embedded_record_count_0x7b == 0U;
    result.wheel_collisions.wheel_damage_flags = input.wheel_damage_flags_0x20;
    constexpr std::size_t wheel_stride_bytes = 0x1a4U;
    for (std::size_t wheel = 0; wheel < input.wheel_collision_inputs.size(); ++wheel) {
        result.wheel_collisions.wheel_subobject_offsets[wheel] =
            wheel * wheel_stride_bytes;
        if (input.wheel_update_prefix_enabled) {
            auto prefix_input = input.wheel_update_prefix_inputs[wheel];
            prefix_input.damage_flag_0x20 =
                result.wheel_collisions.wheel_damage_flags[wheel] != 0U;
            if (input.wheel_spin_preprojection_enabled) {
                prefix_input.rate_0x124 =
                    input.wheel_spin_preprojection_inputs[wheel].spin_rate_0x124;
                prefix_input.wheel_scalar_0x5c =
                    input.wheel_spin_preprojection_inputs[wheel].wheel_scalar_0x5c;
            }
            prefix_input.angle_0xac =
                input.wheel_animation_basis_inputs[wheel].steering_angle_0xac;
            if (input.wheel_contact_pipeline_enabled) {
                const auto& point_transform =
                    input.wheel_update_pipeline_inputs[wheel].point_transform;
                prefix_input.reference_point = point_transform.referenced_point_0x10;
                prefix_input.wheel_position_0x14 =
                    point_transform.wheel_position_0x04_0x0c;
            }
            result.wheel_update_prefix_results[wheel] =
                advance_wheel_update_prefix(prefix_input);
            result.wheel_collisions.wheel_damage_flags[wheel] =
                result.wheel_update_prefix_results[wheel].damage_flag_0x20 ? 1U : 0U;
        }
        auto collision_input = input.wheel_collision_inputs[wheel];
        auto curve_factor_input = input.wheel_tire_curve_factor_inputs[wheel];
        if (input.derive_wheel_tire_setup) {
            curve_factor_input.curves = derive_tire_response_curves(
                input.wheel_tire_definitions[wheel]);
        }
        result.wheel_tire_curves[wheel] = curve_factor_input.curves;
        auto response_vector_input = input.wheel_tire_response_vector_inputs[wheel];
        auto wheel_application_point = input.wheel_application_points_0xc8[wheel];
        if (input.wheel_spin_preprojection_enabled) {
            auto spin_input = input.wheel_spin_preprojection_inputs[wheel];
            spin_input.value_0xe0 =
                result.integrator_wheel_scalars.wheel_values_0xe0[wheel];
            spin_input.damage_flag_0x20 =
                result.wheel_collisions.wheel_damage_flags[wheel];
            if (input.wheel_update_prefix_enabled) {
                spin_input.value_0xa8 =
                    result.wheel_update_prefix_results[wheel].value_0xa8;
                spin_input.use_precomputed_value_0xa8 = true;
            }
            spin_input.state_0x170 =
                input.wheel_tire_dynamics_states[wheel].filtered_component_0x170;
            spin_input.state_0x174 =
                input.wheel_tire_dynamics_states[wheel].filtered_component_0x174;
            spin_input.response_value_0x178 =
                input.wheel_tire_dynamics_states[wheel].suspension_response_0x178;
            spin_input.previous_filter_0x17c =
                input.wheel_tire_dynamics_states[wheel].spin_filter_0x17c;
            const auto& high_speed_axis = input.mode_force_input.body_axis_0x50;
            const auto& linear_velocity = kinematics_at_step_start.linear_velocity;
            spin_input.high_speed_projection =
                high_speed_axis.x * linear_velocity.x +
                high_speed_axis.y * linear_velocity.y +
                high_speed_axis.z * linear_velocity.z;
            result.wheel_spin_preprojection_results[wheel] =
                update_wheel_spin_preprojection(spin_input);
            result.wheel_tire_dynamics_states[wheel].spin_filter_0x17c =
                result.wheel_spin_preprojection_results[wheel].filter_0x17c;
            result.wheel_animation_states[wheel].field_0x12c =
                result.wheel_spin_preprojection_results[wheel].auxiliary_0x12c;
            curve_factor_input.wheel_spin_rate_0x124 =
                result.wheel_spin_preprojection_results[wheel].spin_rate_0x124;
            curve_factor_input.wheel_scalar_0x5c = spin_input.wheel_scalar_0x5c;
            curve_factor_input.derive_rolling_surface_speed = true;
        }
        if (curve_factor_input.derive_rolling_surface_speed) {
            curve_factor_input.rolling_surface_speed =
                curve_factor_input.wheel_spin_rate_0x124 *
                curve_factor_input.wheel_scalar_0x5c;
        }
        if (input.derive_wheel_moment_vectors) {
            auto moment_input = input.wheel_moment_vector_inputs[wheel];
            moment_input.body_basis_0x38_to_0x58 =
                input.wheel_animation_basis_inputs[wheel]
                    .body_basis_0x38_to_0x58;
            if (input.wheel_spin_preprojection_enabled) {
                moment_input.spin_rate_0x124 =
                    result.wheel_spin_preprojection_results[wheel].spin_rate_0x124;
            }
            result.wheel_moment_vector_results[wheel] =
                derive_wheel_moment_vector_0x43e7f0(moment_input);
            result.wheel_moment_vectors_applied[wheel] =
                result.wheel_moment_vector_results[wheel].moment_vector_0x258;
        } else {
            result.wheel_moment_vectors_applied[wheel] =
                input.wheel_moment_vectors_0x258[wheel];
        }
        const auto& moment = result.wheel_moment_vectors_applied[wheel];
        accumulate_ncar_moment_vector(
            result.force_moment_state, {moment.x, moment.y, moment.z});
        if (input.wheel_contact_pipeline_enabled) {
            collision_input.contact_normal = {0.0F, 1.0F, 0.0F};
            collision_input.contact_depth = 0.0F;
            curve_factor_input.contact_normal_components = {0.0F, 1.0F, 0.0F};
            response_vector_input.contact_normal = {0.0F, 1.0F, 0.0F};
            auto wheel_update_input = input.wheel_update_pipeline_inputs[wheel];
            wheel_update_input.feature_state.state_0xec =
                result.wheel_tire_accumulator_states[wheel].state_0xec;
            wheel_update_input.feature_state.state_0xf0 =
                result.wheel_tire_accumulator_states[wheel].state_0xf0;
            wheel_update_input.feature_state.response_gate_0x11c =
                result.wheel_tire_dynamics_states[wheel].response_gate_0x11c;
            const auto point_velocity = derive_body_point_velocity_0x43e9b0({
                kinematics_at_step_start.position,
                result.wheel_contact_states[wheel].contact_point_0x130,
                kinematics_at_step_start.linear_velocity,
                kinematics_at_step_start.angular_velocity});
            result.wheel_point_velocities_0x43e9b0[wheel] = point_velocity;
            wheel_update_input.suspension.surface.signed_motion_projection =
                project_body_point_velocity_0x442530(point_velocity,
                    wheel_update_input.point_transform.body_transform_0x38_to_0x64);
            result.wheel_contact_pipeline_results[wheel] = advance_wheel_update_pipeline(
                result.wheel_contact_states[wheel],
                wheel_update_input);
            wheel_application_point = transform_wheel_point_0xc8(
                wheel_update_input.point_transform);
            const auto& update = result.wheel_contact_pipeline_results[wheel];
            result.wheel_tire_accumulator_states[wheel].state_0xec =
                update.feature_state.state_0xec;
            result.wheel_tire_accumulator_states[wheel].state_0xf0 =
                update.feature_state.state_0xf0;
            result.wheel_tire_dynamics_states[wheel].response_gate_0x11c =
                update.feature_state.response_gate_0x11c;
            curve_factor_input.reference_speed_scale_0x1c =
                update.feature_profile.response_scale;
            curve_factor_input.use_reference_speed_scale_0x1c = true;
            if (update.contact.reached_query) {
                if (wheel_update_input.contact.query_succeeded) {
                    collision_input.contact_normal =
                        wheel_update_input.contact.query_contact_normal;
                    curve_factor_input.contact_normal_components = {
                        collision_input.contact_normal.x,
                        collision_input.contact_normal.y,
                        collision_input.contact_normal.z};
                    response_vector_input.contact_normal =
                        collision_input.contact_normal;
                }
                if (update.contact.contact_gate_passed) {
                    collision_input.contact_depth = update.contact.clamped_contact_depth;
                } else {
                    collision_input.contact_depth = 0.0F;
                }
            }
            const auto& suspension_force = update.suspension.finalized.applied_force;
            curve_factor_input.suspension_response_components = {
                suspension_force.x, suspension_force.y, suspension_force.z};

            const auto& forward = response_vector_input.forward_basis;
            const auto& normal = response_vector_input.contact_normal;
            const PhysicsVector3 lateral{
                normal.y * forward.z - normal.z * forward.y,
                normal.z * forward.x - normal.x * forward.z,
                normal.x * forward.y - normal.y * forward.x};
            curve_factor_input.lateral_velocity =
                point_velocity.x * lateral.x + point_velocity.y * lateral.y +
                point_velocity.z * lateral.z;
            curve_factor_input.longitudinal_velocity =
                point_velocity.x * forward.x + point_velocity.y * forward.y +
                point_velocity.z * forward.z;
            curve_factor_input.slip_curve_input =
                curve_factor_input.rolling_surface_speed -
                curve_factor_input.longitudinal_velocity;
        }
        // These collision arguments are read from the body and wheel records
        // by 0x442530. Reuse the state already carried by the body step and
        // contact pipeline rather than requiring duplicate caller snapshots.
        collision_input.body_center = kinematics_at_step_start.position;
        collision_input.body_velocity = kinematics_at_step_start.linear_velocity;
        if (input.wheel_contact_pipeline_enabled) {
            collision_input.contact_point =
                result.wheel_contact_states[wheel].contact_point_0x130;
        }
        result.wheel_collisions.wheel_results[wheel] = apply_wheel_collision_impulse(
            result.point_impulse_state,
            result.wheel_collisions.wheel_damage_flags[wheel],
            collision_input);
        auto animation_basis_input = input.wheel_animation_basis_inputs[wheel];
        if (input.wheel_update_prefix_enabled) {
            animation_basis_input.steering_angle_0xac =
                result.wheel_update_prefix_results[wheel].angle_0xac;
        }
        animation_basis_input.damage_flag_0x20 =
            result.wheel_collisions.wheel_damage_flags[wheel] != 0U;
        if (input.wheel_contact_pipeline_enabled) {
            animation_basis_input.contact_depth_0xe8 =
                result.wheel_contact_pipeline_results[wheel]
                    .suspension.finalized.next_contact_depth_0xe8;
        }
        result.wheel_animation_basis_states[wheel] = advance_wheel_animation_basis(
            animation_basis_input);
        result.wheel_moment_local_axes_next_0xb0[wheel] = {
            result.wheel_animation_basis_states[wheel].sine_0xb0,
            result.wheel_animation_basis_states[wheel].zero_0xb4,
            result.wheel_animation_basis_states[wheel].cosine_0xb8};
        if (input.wheel_contact_pipeline_enabled) {
            const auto& suspension_force = result.wheel_contact_pipeline_results[wheel]
                .suspension.finalized.applied_force;
            const std::array<float, 3> force{
                suspension_force.x, suspension_force.y, suspension_force.z};
            const std::array<float, 3> application_point{
                wheel_application_point.x,
                wheel_application_point.y,
                wheel_application_point.z};
            const std::array<float, 3> body_center{
                collision_input.body_center.x,
                collision_input.body_center.y,
                collision_input.body_center.z};
            result.wheel_suspension_force_accumulated[wheel] =
                accumulate_ncar_force_moment(
                    result.force_moment_state, force, application_point, body_center);
        }
        result.wheel_application_points_applied_0xc8[wheel] = wheel_application_point;
        const auto response_derivation = derive_tire_wheel_response(
            response_vector_input.factors, curve_factor_input);
        auto dynamics_input = input.wheel_tire_dynamics_inputs[wheel];
        if (input.derive_wheel_tire_setup) {
            dynamics_input.tire_terms = derive_tire_trigonometric_terms(
                input.wheel_tire_definitions[wheel]);
        }
        dynamics_input.instantaneous_component_0x2c =
            response_derivation.dynamics_component_0x2c;
        dynamics_input.instantaneous_component_0x30 =
            response_derivation.dynamics_component_0x30;
        dynamics_input.secondary_slip_component_0xe4 =
            response_derivation.secondary_slip_component_0xe4;
        dynamics_input.clear_filter_state = dynamics_input.clear_filter_state ||
            !response_derivation.dynamics_division_valid;
        if (input.wheel_contact_pipeline_enabled) {
            result.wheel_tire_dynamics_states[wheel].suspension_response_0x178 =
                result.wheel_contact_pipeline_results[wheel]
                    .suspension.surface.adjusted_response;
        }
        result.wheel_tire_dynamics_results[wheel] = advance_wheel_tire_dynamics(
            result.wheel_tire_dynamics_states[wheel], dynamics_input);
        auto evolved_response_factors = response_derivation.factors;
        evolved_response_factors.trigonometric_response =
            result.wheel_tire_dynamics_results[wheel].trigonometric_response;
        const auto response_components =
            combine_tire_wheel_response(evolved_response_factors);
        if (input.wheel_tire_accumulators_enabled) {
            auto accumulator_input = input.wheel_tire_accumulator_inputs[wheel];
            const auto& accumulator_state =
                result.wheel_tire_accumulator_states[wheel];
            accumulator_input.accumulator_0xa4 =
                accumulator_state.accumulator_0xa4;
            accumulator_input.accumulator_0x10c =
                accumulator_state.accumulator_0x10c;
            accumulator_input.state_0xec = accumulator_state.state_0xec;
            accumulator_input.state_0xf0 = accumulator_state.state_0xf0;
            accumulator_input.forward_response_local_0x38 =
                response_components.forward;
            accumulator_input.lateral_response_local_0x2c =
                response_components.lateral;
            if (input.wheel_contact_pipeline_enabled) {
                accumulator_input.divisor_0x1c =
                    result.wheel_contact_pipeline_results[wheel]
                        .feature_profile.response_scale;
            }
            accumulator_input.response_gate_0x11c =
                result.wheel_tire_dynamics_states[wheel].response_gate_0x11c;
            if (input.wheel_contact_pipeline_enabled) {
                accumulator_input.contact_active_0x168 =
                    result.wheel_contact_states[wheel].contact_active_0x168 != 0U;
            }
            result.wheel_tire_accumulator_results[wheel] =
                advance_wheel_tire_accumulators(accumulator_input);
            const auto& accumulator_result =
                result.wheel_tire_accumulator_results[wheel];
            result.wheel_tire_accumulator_states[wheel] = {
                accumulator_result.accumulator_0xa4,
                accumulator_result.accumulator_0x10c,
                accumulator_result.state_0xec,
                accumulator_result.state_0xf0};
        }
        response_vector_input.factors = evolved_response_factors;
        result.wheel_tire_response_vectors[wheel] = derive_wheel_tire_response_vector(
            response_vector_input);
        auto tire_basis_input = input.wheel_tire_basis_update_inputs[wheel];
        tire_basis_input.response_vector_after_scaling =
            result.wheel_tire_response_vectors[wheel];
        tire_basis_input.basis_0x110_0x118 = {
            result.wheel_animation_basis_states[wheel].basis_0x110_0x118[0],
            result.wheel_animation_basis_states[wheel].basis_0x110_0x118[1],
            result.wheel_animation_basis_states[wheel].basis_0x110_0x118[2]};
        result.wheel_tire_basis_updates[wheel] = apply_wheel_tire_basis_update(
            tire_basis_input);
        auto secondary_force_input = input.wheel_secondary_body_forces[wheel];
        if (input.wheel_contact_pipeline_enabled) {
            secondary_force_input.factor_0x78 =
                result.wheel_contact_pipeline_results[wheel]
                    .feature_profile.secondary_response;
        }
        result.wheel_body_forces[wheel] = accumulate_wheel_body_force_sequence(
            result.force_moment_state,
            result.wheel_tire_basis_updates[wheel],
            wheel_application_point,
            collision_input.body_center,
            secondary_force_input);
        auto conditional_tire_input = input.wheel_conditional_tire_force_inputs[wheel];
        conditional_tire_input.tire_scalar_0x10 =
            curve_factor_input.curves.quadratic.quadratic;
        conditional_tire_input.corrected_tire_vector_0x100 =
            result.wheel_tire_basis_updates[wheel].corrected_vector_0x100_0x108;
        conditional_tire_input.body_center_0x5c =
            collision_input.body_center;
        conditional_tire_input.body_velocity_0x234 =
            collision_input.body_velocity;
        conditional_tire_input.response_scale = 1.0F;
        if (input.wheel_contact_pipeline_enabled) {
            conditional_tire_input.feature_gate_0x168 =
                result.wheel_contact_states[wheel].contact_active_0x168 != 0U;
            conditional_tire_input.low_traction_flag_0x19c =
                result.wheel_contact_states[wheel].contact_feature_0x19c != 0;
        }
        result.wheel_conditional_tire_forces[wheel] =
            apply_wheel_conditional_tire_force(
                result.force_moment_state,
                input.mode_force_input.mode_0x4f4,
                conditional_tire_input);
        auto finalization_input = input.wheel_rate_finalization_inputs[wheel];
        if (input.wheel_contact_pipeline_enabled) {
            finalization_input.animation_flags_0x120 =
                result.wheel_contact_pipeline_results[wheel]
                    .feature_state.animation_flags_0x120;
        }
        if (input.wheel_spin_preprojection_enabled) {
            finalization_input.rate_0x124 =
                result.wheel_spin_preprojection_results[wheel].spin_rate_0x124;
            finalization_input.blend_local_0x28 =
                result.wheel_spin_preprojection_results[wheel].blend_local_0x28;
            finalization_input.response_local_0x90 =
                result.wheel_spin_preprojection_results[wheel].response_local_0x90;
        }
        const auto& tire_vector = result.wheel_tire_response_vectors[wheel];
        const auto& wheel_forward = response_vector_input.forward_basis;
        finalization_input.response_local_0x90 -=
            tire_vector.x * wheel_forward.x +
            tire_vector.y * wheel_forward.y +
            tire_vector.z * wheel_forward.z;
        result.wheel_rate_response_local_0x90[wheel] =
            finalization_input.response_local_0x90;
        finalization_input.damage_flag_0x20 =
            result.wheel_collisions.wheel_damage_flags[wheel];
        result.wheel_rate_finalization_results[wheel] = finalize_wheel_rate_state(
            finalization_input);
        ++result.wheel_collisions.wheel_calls;
    }

    result.integrator_bookkeeping = advance_ncar_integrator_bookkeeping(
        result.integrator_bookkeeping_state,
        input.integrator_bookkeeping_input);
    if (result.integrator_bookkeeping.substep_helper_0x4323e0_reached) {
        result.periodic_animation = advance_vehicle_animation_frame(
            result.wheel_animation_states,
            result.vehicle_animation_channel_0x18,
            result.vehicle_animation_channel_0x5c,
            input.animation_current_time,
            result.integrator_bookkeeping.substep_helper_argument);
    }

    auto mode_force_input = input.mode_force_input;
    mode_force_input.leading_wheel_damage_bytes = {
        result.wheel_collisions.wheel_damage_flags[0],
        result.wheel_collisions.wheel_damage_flags[1]};
    mode_force_input.angular_velocity_0x240 = kinematics_at_step_start.angular_velocity;
    mode_force_input.linear_velocity_0x234 = kinematics_at_step_start.linear_velocity;
    mode_force_input.body_center = kinematics_at_step_start.position;
    if (input.wheel_contact_pipeline_enabled) {
        for (std::size_t wheel = 0; wheel < mode_force_input.contact_bytes.size(); ++wheel) {
            mode_force_input.contact_bytes[wheel] =
                result.wheel_contact_states[wheel].contact_active_0x168;
        }
    }
    result.mode_force = apply_ncar_integrator_mode_force(
        result.force_moment_state, mode_force_input);
    auto wheel_pair_input = input.wheel_pair_differential_input;
    wheel_pair_input.contact_bytes = mode_force_input.contact_bytes;
    result.wheel_pair_differentials =
        apply_ncar_integrator_wheel_pair_differentials(wheel_pair_input);
    auto common_force_input = input.common_force_input;
    common_force_input.contact_bytes = mode_force_input.contact_bytes;
    common_force_input.body_center = kinematics_at_step_start.position;
    result.common_force = apply_ncar_integrator_common_force(
        result.force_moment_state, common_force_input);
    auto velocity_force_input = input.velocity_force_input;
    velocity_force_input.velocity_vector = kinematics_at_step_start.linear_velocity;
    velocity_force_input.body_center = kinematics_at_step_start.position;
    result.velocity_forces = apply_ncar_integrator_velocity_forces(
        result.force_moment_state, velocity_force_input);
    if (input.attached_object_force_gate_0xbe4) {
        constexpr float radians_5 = 0.08726646F;
        constexpr float radians_2 = 0.034906585F;
        constexpr float radians_7 = 0.12217305F;
        result.attached_object_targets_0x38 = {
            input.attached_object_tuning_0x500 * radians_5 + radians_2,
            radians_2 - input.attached_object_tuning_0x500 * radians_5,
            -input.attached_object_tuning_0x508 * radians_7,
            0.0F};
        for (std::size_t index = 0;
             index < result.attached_object_forces.size(); ++index) {
            auto object_input = input.attached_object_force_inputs[index];
            object_input.target_projection_0x38 =
                result.attached_object_targets_0x38[index];
            object_input.body_center = kinematics_at_step_start.position;
            result.attached_object_forces[index] = apply_ncar_attached_object_force(
                result.force_moment_state, object_input);
        }
    }

    if (input.embedded_record_count_0x7b != 0U) {
        if (input.terrain == nullptr) {
            result.embedded_records.requested_records =
                input.embedded_record_count_0x7b;
            result.embedded_records.unprocessed_records =
                input.embedded_record_count_0x7b;
            result.embedded_records.input_complete = false;
        } else {
            auto record_body_input = input.embedded_record_body_input;
            record_body_input.body_motion.linear_0x234 = {
                kinematics_at_step_start.linear_velocity.x,
                kinematics_at_step_start.linear_velocity.y,
                kinematics_at_step_start.linear_velocity.z};
            record_body_input.body_motion.angular_0x240 = {
                kinematics_at_step_start.angular_velocity.x,
                kinematics_at_step_start.angular_velocity.y,
                kinematics_at_step_start.angular_velocity.z};
            record_body_input.body_motion.center_0x5c = {
                kinematics_at_step_start.position.x,
                kinematics_at_step_start.position.y,
                kinematics_at_step_start.position.z};
            result.embedded_records = update_embedded_record_contact_batch_from_terrain(
                input.embedded_record_states,
                input.embedded_record_local_points_0x00,
                input.embedded_record_count_0x7b,
                result.energy_accumulator_0xe40,
                result.force_moment_state,
                record_body_input,
                *input.terrain,
                input.compatibility_division_enabled);
        }
        result.input_complete = result.input_complete &&
            result.embedded_records.input_complete;
    }

    BodyIntegrationForceCollectionInput force_collection_input{};
    force_collection_input.embedded_record_accumulator_delta = {
        result.force_moment_state.force_0x24c[0],
        result.force_moment_state.force_0x24c[1],
        result.force_moment_state.force_0x24c[2],
        result.force_moment_state.moment_0x258[0],
        result.force_moment_state.moment_0x258[1],
        result.force_moment_state.moment_0x258[2]};
    force_collection_input.packed_virtual_records = input.packed_virtual_records;
    result.force_collection = prepare_body_integration_force_collection(
        result.body_tail_state,
        result.point_impulse_state,
        input.kinematics.gravity_mass_0x7c,
        force_collection_input);
    result.input_complete = result.input_complete &&
        result.force_collection.virtual_record_calls.truncated_records == 0U;
    result.integrator_virtual_record_callbacks.reserve(
        result.force_collection.virtual_record_calls.calls.size());
    for (const auto& record : result.force_collection.virtual_record_calls.calls) {
        const NcarIntegratorVirtualRecordCallbackInput callback_input{
            record,
            input.post_body_input.body_local_offset_0x28,
            kinematics_at_step_start.orientation_matrix_0x0e,
            {kinematics_at_step_start.position.x,
             kinematics_at_step_start.position.y,
             kinematics_at_step_start.position.z},
            input.post_body_input.global_update_gate_0x425870,
            input.post_body_input.fdiv_compatibility_enabled,
            input.post_body_input.integrator_mesh_vertices,
            input.post_body_input.integrator_mesh_vertex_buffer_resolved,
            input.post_body_input.integrator_mesh_vertex_index_begin,
            input.post_body_input.integrator_mesh_vertex_index_end,
            input.post_body_input.integrator_mesh_vertex_index_range_resolved,
            input.post_body_input.integrator_mesh_descriptor_pointer_0x10,
            input.post_body_input.integrator_mesh_groups,
            input.post_body_input.current_time_0x425790,
            input.post_body_input.vehicle_ordinal_0x510};
        auto callback_result = apply_ncar_integrator_virtual_record_callback(
            result.post_body_state, callback_input);
        if (callback_result.secondary_virtual_call_reached &&
            !callback_result.mesh_response.input_complete) {
            result.input_complete = false;
        }
        result.integrator_virtual_record_callbacks.push_back(
            std::move(callback_result));
    }

    auto kinematics_input = kinematics_at_step_start;
    const auto& accumulated =
        result.body_tail_state.running_accumulators_0x93_to_0x98;
    kinematics_input.accumulated_linear_force = {
        accumulated[0], accumulated[1], accumulated[2]};
    kinematics_input.accumulated_angular_moment = {
        accumulated[3], accumulated[4], accumulated[5]};
    result.kinematics = advance_body_integration_kinematics(
        result.body_tail_state, kinematics_input);
    auto post_body_input = input.post_body_input;
    post_body_input.derive_vector_0x382 = true;
    post_body_input.orientation_matrix_0x38 =
        result.kinematics.orientation.orientation_matrix;
    post_body_input.body_axis_0x14 = {
        result.kinematics.orientation.orientation_matrix[6],
        result.kinematics.orientation.orientation_matrix[7],
        result.kinematics.orientation.orientation_matrix[8]};
    post_body_input.reference_axis_0x44 = {
        result.kinematics.orientation.orientation_matrix[3],
        result.kinematics.orientation.orientation_matrix[4],
        result.kinematics.orientation.orientation_matrix[5]};
    post_body_input.angular_velocity_0xa2 = {
        result.kinematics.angular.angular_velocity.x,
        result.kinematics.angular.angular_velocity.y,
        result.kinematics.angular.angular_velocity.z};
    post_body_input.linear_velocity_0x234 = {
        result.kinematics.linear.velocity.x,
        result.kinematics.linear.velocity.y,
        result.kinematics.linear.velocity.z};
    post_body_input.wheel_contact_bytes = mode_force_input.contact_bytes;
    post_body_input.wear_value_bits = mode_force_input.wear_value_bits;
    result.post_body = advance_ncar_post_body_tail(
        result.post_body_state, post_body_input);
    result.input_complete = result.input_complete &&
        result.post_body.resource_group_inputs_complete;
    if (result.post_body.wheel_damage_flags_cleared) {
        result.wheel_collisions.wheel_damage_flags.fill(0U);
    }
    return result;
}

} // namespace viper
