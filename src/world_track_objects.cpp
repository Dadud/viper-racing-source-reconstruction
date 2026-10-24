#include "viper/world_track_objects.hpp"
#include "viper/resource_bundle.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace viper {
namespace {

bool equals_ascii_case_insensitive(
    const std::string_view left,
    const std::string_view right) noexcept
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        auto left_char = left[index];
        auto right_char = right[index];
        if (left_char >= 'A' && left_char <= 'Z') {
            left_char = static_cast<char>(left_char - 'A' + 'a');
        }
        if (right_char >= 'A' && right_char <= 'Z') {
            right_char = static_cast<char>(right_char - 'A' + 'a');
        }
        if (left_char != right_char) {
            return false;
        }
    }
    return true;
}

std::uint32_t read_u32_le(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1])) << 8U) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2])) << 16U) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3])) << 24U);
}

void write_u32_le(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint32_t value) noexcept
{
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<std::byte>(
            (value >> (index * 8U)) & 0xffU);
    }
}

std::uint8_t read_u8(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

void write_u8(
    const std::span<std::byte> bytes,
    const std::size_t offset,
    const std::uint8_t value) noexcept
{
    bytes[offset] = static_cast<std::byte>(value);
}

} // namespace

std::optional<TrackObjectTextTable> parse_track_object_text_table(
    const std::span<const std::byte> payload)
{
    constexpr auto header_size = TrackObjectTextTable::header_size;
    if (payload.size() < header_size) {
        return std::nullopt;
    }

    TrackObjectTextTable table;
    table.line_count = read_u32_le(payload, 0);
    table.raw_header_word_0x04 = read_u32_le(payload, 4);
    for (std::size_t index = 0; index < table.line_offsets.size(); ++index) {
        table.line_offsets[index] = read_u32_le(payload, 8 + index * 4);
    }
    table.line_stride = read_u32_le(payload, 0x48);

    if (table.line_count != 0 && table.line_stride == 0) {
        return std::nullopt;
    }
    const auto maximum_count =
        (std::numeric_limits<std::size_t>::max() - header_size) /
        (table.line_stride == 0 ? 1U : table.line_stride);
    if (table.line_count > maximum_count) {
        return std::nullopt;
    }
    const auto expected_size = header_size +
        static_cast<std::size_t>(table.line_count) * table.line_stride;
    if (payload.size() != expected_size) {
        return std::nullopt;
    }
    if (table.line_count != 0 && std::any_of(
            table.line_offsets.begin(), table.line_offsets.end(),
            [&table](const std::uint32_t offset) {
                return offset >= table.line_stride;
            })) {
        return std::nullopt;
    }

    table.raw_payload.assign(payload.begin(), payload.end());
    return table;
}

std::optional<IdealLineResourcePayload> parse_ideal_line_resource(
    const std::uint32_t resource_version,
    const std::span<const std::byte> payload)
{
    constexpr auto header_size = IdealLineResourcePayload::header_size;
    constexpr auto stride = IdealLineResourcePayload::record_stride;
    if (resource_version != IdealLineResourcePayload::supported_resource_version ||
        payload.size() < header_size ||
        read_u32_le(payload, 0U) != IdealLineResourcePayload::internal_marker ||
        read_u32_le(payload, 4U) != stride) {
        return std::nullopt;
    }

    IdealLineResourcePayload result;
    result.resource_version = resource_version;
    result.line_count = read_u32_le(payload, 8U);
    const auto available_bytes = payload.size() - header_size;
    if (result.line_count > available_bytes / stride) {
        return std::nullopt;
    }

    result.records.resize(result.line_count);
    for (std::size_t index = 0; index < result.records.size(); ++index) {
        const auto source_offset = header_size + index * stride;
        std::copy_n(payload.begin() + static_cast<std::ptrdiff_t>(source_offset),
                    stride, result.records[index].begin());
    }
    const auto records_end = header_size + result.records.size() * stride;
    result.trailing_bytes.assign(
        payload.begin() + static_cast<std::ptrdiff_t>(records_end), payload.end());
    return result;
}

std::optional<IdealLineTrackData> normalize_ideal_line_resource(
    const IdealLineResourcePayload& payload,
    const std::uint8_t orientation_mode)
{
    if (payload.resource_version != IdealLineResourcePayload::supported_resource_version ||
        payload.records.size() != payload.line_count || payload.records.empty()) {
        return std::nullopt;
    }

    IdealLineTrackData result;
    result.records = payload.records;
    result.trailing_bytes = payload.trailing_bytes;
    result.orientation_normalized = orientation_mode != 0U;
    if (result.orientation_normalized && result.records.size() > 1U) {
        std::reverse(result.records.begin() + 1, result.records.end());
    }

    const auto read_record_word = [](const IdealLineTrackData::Record& record,
                                     const std::size_t offset) {
        return read_u32_le(std::span<const std::byte>(record), offset);
    };
    const auto write_record_word = [](IdealLineTrackData::Record& record,
                                      const std::size_t offset,
                                      const std::uint32_t value) {
        write_u32_le(std::span<std::byte>(record), offset, value);
    };
    const auto read_record_byte = [](const IdealLineTrackData::Record& record,
                                     const std::size_t offset) {
        return read_u8(std::span<const std::byte>(record), offset);
    };
    const auto write_record_byte = [](IdealLineTrackData::Record& record,
                                      const std::size_t offset,
                                      const std::uint8_t value) {
        write_u8(std::span<std::byte>(record), offset, value);
    };

    if (result.orientation_normalized) {
        const auto first_duration = std::bit_cast<float>(
            read_record_word(result.records.front(), 0x20U));
        float cumulative_duration = 0.0F;
        for (std::size_t index = 0; index + 1U < result.records.size(); ++index) {
            auto& current = result.records[index];
            write_record_word(current, 0x24U,
                std::bit_cast<std::uint32_t>(cumulative_duration));
            const auto next_duration = std::bit_cast<float>(
                read_record_word(result.records[index + 1U], 0x20U));
            write_record_word(current, 0x20U,
                std::bit_cast<std::uint32_t>(next_duration));
            cumulative_duration = next_duration + cumulative_duration;
        }
        auto& last = result.records.back();
        write_record_word(last, 0x24U,
            std::bit_cast<std::uint32_t>(cumulative_duration));
        write_record_word(last, 0x20U,
            std::bit_cast<std::uint32_t>(first_duration));

        for (auto& record : result.records) {
            write_record_word(record, 0x0cU,
                read_record_word(record, 0x0cU) ^ 0x80000000U);
            write_record_word(record, 0x10U,
                read_record_word(record, 0x10U) ^ 0x80000000U);
        }

        // Native code advances a byte pointer by 0x44 while reading at +0x7a
        // from the resource header, i.e. +0x6e from the record array. This
        // intentionally aliases the next row's +0x2a field. Model that flat
        // address explicitly instead of indexing beyond each std::array.
        const auto read_category_label = [&result, &read_record_byte](
            const std::size_t record_index)
            -> std::optional<std::uint8_t> {
            const auto flat_offset = record_index *
                static_cast<std::size_t>(IdealLineResourcePayload::record_stride) +
                0x6eU;
            const auto records_size = result.records.size() *
                static_cast<std::size_t>(IdealLineResourcePayload::record_stride);
            if (flat_offset < records_size) {
                const auto row = flat_offset /
                    IdealLineResourcePayload::record_stride;
                const auto in_row = flat_offset %
                    IdealLineResourcePayload::record_stride;
                return read_record_byte(result.records[row], in_row);
            }
            const auto tail_offset = flat_offset - records_size;
            if (tail_offset < result.trailing_bytes.size()) {
                return read_u8(result.trailing_bytes, tail_offset);
            }
            return std::nullopt;
        };
        const auto write_category_label = [&result, &write_record_byte](
            const std::size_t record_index, const std::uint8_t value) {
            const auto flat_offset = record_index *
                static_cast<std::size_t>(IdealLineResourcePayload::record_stride) +
                0x6eU;
            const auto records_size = result.records.size() *
                static_cast<std::size_t>(IdealLineResourcePayload::record_stride);
            if (flat_offset < records_size) {
                const auto row = flat_offset /
                    IdealLineResourcePayload::record_stride;
                const auto in_row = flat_offset %
                    IdealLineResourcePayload::record_stride;
                write_record_byte(result.records[row], in_row, value);
                return;
            }
            const auto tail_offset = flat_offset - records_size;
            if (tail_offset < result.trailing_bytes.size()) {
                result.trailing_bytes[tail_offset] = static_cast<std::byte>(value);
            }
        };

        const auto first_label = read_category_label(0U);
        if (first_label && *first_label != 0U && *first_label < 5U) {
            result.category_labels_normalized = true;
            write_record_byte(result.records.front(), 0x2aU, 1U);
            auto previous_label = *first_label;
            std::uint8_t normalized_label = 1U;
            for (std::size_t index = 0; index + 1U < result.records.size(); ++index) {
                const auto label = read_category_label(index);
                if (!label) {
                    result.category_label_input_incomplete = true;
                    break;
                }
                if (*label != previous_label) {
                    ++normalized_label;
                    previous_label = *label;
                }
                write_category_label(index, normalized_label);
            }
        } else if (!first_label) {
            result.category_label_input_incomplete = true;
        }
    }

    result.next_indices.resize(result.records.size());
    for (std::size_t index = 0; index < result.records.size(); ++index) {
        result.next_indices[index] = (index + 1U) % result.records.size();
        if (index + 1U < result.records.size() &&
            read_record_word(result.records[index], 0x3cU) == 0xFEEDBEEFU) {
            write_record_word(result.records[index], 0x3cU, 0U);
        }
        const auto row_index = static_cast<std::uint16_t>(index);
        write_record_byte(result.records[index], 0x28U,
            static_cast<std::uint8_t>(row_index & 0xffU));
        write_record_byte(result.records[index], 0x29U,
            static_cast<std::uint8_t>(row_index >> 8U));
    }
    return result;
}

std::optional<std::string_view> track_object_text_line(
    const TrackObjectTextTable& table,
    const std::size_t line_index,
    const std::size_t channel) noexcept
{
    if (line_index >= table.line_count ||
        channel >= table.line_offsets.size() ||
        table.line_offsets[channel] >= table.line_stride) {
        return std::nullopt;
    }

    const auto line_offset = static_cast<std::size_t>(table.line_offsets[channel]);
    if (table.line_stride == 0 ||
        line_index > (std::numeric_limits<std::size_t>::max() -
                      TrackObjectTextTable::header_size - line_offset) /
                         table.line_stride) {
        return std::nullopt;
    }
    const auto offset = table.header_size + line_index * table.line_stride +
        line_offset;
    if (offset >= table.raw_payload.size()) {
        return std::nullopt;
    }
    const auto* begin = reinterpret_cast<const char*>(
        table.raw_payload.data() + static_cast<std::ptrdiff_t>(offset));
    const auto available = static_cast<std::size_t>(table.line_stride) -
        line_offset;
    if (available > table.raw_payload.size() - offset) {
        return std::nullopt;
    }
    const auto* end = std::find(begin, begin + available, '\0');
    return std::string_view(begin, static_cast<std::size_t>(end - begin));
}

TrackDirectiveObservation classify_track_directive(
    std::string_view line) noexcept
{
    const auto original_line = line;
    const auto is_space = [](const char value) noexcept {
        return value == ' ' || value == '\t' || value == '\r' ||
               value == '\n' || value == '\f' || value == '\v';
    };
    while (!line.empty() && is_space(line.front())) {
        line.remove_prefix(1);
    }
    if (line.empty()) {
        return {TrackDirectiveKind::empty_or_unscannable, {}};
    }

    const auto token_end = std::find_if(line.begin(), line.end(), is_space);
    const std::string_view token(line.data(),
        static_cast<std::size_t>(token_end - line.begin()));
    if (token.front() == ';') {
        return {TrackDirectiveKind::comment, {}};
    }
    if (equals_ascii_case_insensitive(token, "end")) {
        return {TrackDirectiveKind::end, {}};
    }
    if (equals_ascii_case_insensitive(token, "map")) {
        return {TrackDirectiveKind::map, {}};
    }
    if (equals_ascii_case_insensitive(token, "ai")) {
        return {TrackDirectiveKind::ai, {}};
    }
    if (equals_ascii_case_insensitive(token, "obj")) {
        return {TrackDirectiveKind::object, original_line};
    }
    return {TrackDirectiveKind::bad_token, {}};
}

TrackTransformPlan plan_track_transform_from_checkpoint(
    const std::span<const TrackTransformCheckpointCandidate> candidates,
    const bool centered_half_turn_enabled) noexcept
{
    TrackTransformPlan plan;
    plan.transform.basis = {1.0F, 0.0F, 0.0F,
                            0.0F, 1.0F, 0.0F,
                            0.0F, 0.0F, 1.0F};

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto& candidate = candidates[index];
        if (candidate.scanf_conversion_count <= 3U) {
            continue;
        }
        plan.checkpoint_candidate_index = index + 1U;
        if (!centered_half_turn_enabled) {
            plan.disposition = TrackTransformDisposition::identity_from_checkpoint;
            return plan;
        }
        if (!candidate.second_z.has_value()) {
            plan.disposition =
                TrackTransformDisposition::centered_half_turn_missing_coordinate;
            plan.uses_uninitialized_fourth_coordinate = true;
            return plan;
        }

        constexpr float pi = 3.1415927F;
        const auto angle = static_cast<long double>(pi);
        const auto cosine = static_cast<float>(std::cos(angle));
        const auto sine = static_cast<float>(std::sin(angle));
        plan.transform.basis = {cosine, 0.0F, -sine,
                                0.0F, 1.0F, 0.0F,
                                sine, 0.0F, cosine};

        const auto center_x = (candidate.first_x + candidate.second_x) * 0.5F;
        const auto center_z = (candidate.first_z + *candidate.second_z) * 0.5F;
        const auto& m = plan.transform.basis;
        const auto tx = -static_cast<long double>(center_x) +
            static_cast<long double>(m[0]) * center_x +
            static_cast<long double>(m[3]) * 0.0L +
            static_cast<long double>(m[6]) * center_z;
        const auto ty = 0.0L + static_cast<long double>(m[7]) * center_z +
            static_cast<long double>(m[1]) * center_x +
            static_cast<long double>(m[4]) * 0.0L;
        const auto tz = -static_cast<long double>(center_z) +
            static_cast<long double>(m[8]) * center_z +
            static_cast<long double>(m[2]) * center_x +
            static_cast<long double>(m[5]) * 0.0L;
        plan.transform.translation = {
            static_cast<float>(-tx), static_cast<float>(-ty),
            static_cast<float>(-tz),
        };
        plan.disposition = TrackTransformDisposition::centered_half_turn;
        plan.x87_rounding_approximated = true;
        return plan;
    }

    plan.disposition = TrackTransformDisposition::checkpoint_not_found_fatal;
    return plan;
}

float compute_track_geometry_key(
    const TrackGeometryKeyObservation& observation) noexcept
{
    if (!observation.projection_key_enabled) {
        return 0.0F;
    }
    return (observation.object_z_0x1c - observation.reference_z_0x40) *
               observation.z_weight_0x50 +
           (observation.object_x_0x18 - observation.reference_x_0x3c) *
               observation.x_weight_0x4c;
}

CarSplineTargetKeyPlan plan_car_spline_target_key(
    const float placement_base_key,
    const std::int32_t placement_ordinal) noexcept
{
    const auto ordinal_offset =
        static_cast<float>(placement_ordinal) * 15.0F + 28.0F;
    return {placement_base_key, ordinal_offset,
            placement_base_key - ordinal_offset};
}

std::int32_t compute_car_placement_ordinal(
    const std::int32_t global_car_index,
    const std::int32_t gcar_count_before,
    const bool current_car_is_gcar) noexcept
{
    const auto gcar_count_after =
        static_cast<std::uint32_t>(gcar_count_before) +
        static_cast<std::uint32_t>(current_car_is_gcar);
    const auto ordinal_bits = static_cast<std::uint32_t>(global_car_index) -
        gcar_count_after;
    return std::bit_cast<std::int32_t>(ordinal_bits);
}

TrackSplineSelectionPlan select_track_spline_segment(
    const TrackSplineSelectionObservation& observation) noexcept
{
    if (!observation.geometry_loaded) {
        return {TrackSplineSelectionDisposition::geometry_unavailable,
                std::nullopt, 0.0F, false};
    }
    if (observation.head_index >= observation.nodes.size()) {
        return {TrackSplineSelectionDisposition::invalid_head,
                std::nullopt, 0.0F, false};
    }

    auto current = observation.head_index;
    for (std::size_t step = 0; step <= observation.nodes.size(); ++step) {
        const auto next = observation.nodes[current].next_index;
        if (next >= observation.nodes.size()) {
            return {TrackSplineSelectionDisposition::invalid_link,
                    std::nullopt, 0.0F, false};
        }
        if (next == observation.head_index) {
            const auto& selected = observation.nodes[current];
            return {TrackSplineSelectionDisposition::selected, current,
                    (observation.target_key - selected.segment_key) /
                        selected.segment_duration,
                    true};
        }
        if (!(observation.nodes[next].segment_key < observation.target_key)) {
            const auto& selected = observation.nodes[current];
            return {TrackSplineSelectionDisposition::selected, current,
                    (observation.target_key - selected.segment_key) /
                        selected.segment_duration,
                    true};
        }
        current = next;
    }
    return {TrackSplineSelectionDisposition::invalid_cycle,
            std::nullopt, 0.0F, false};
}

std::vector<TrackSplineIndexNode> make_track_spline_index_nodes(
    const IdealLineTrackData& track_data)
{
    if (track_data.records.size() != track_data.next_indices.size()) {
        return {};
    }

    std::vector<TrackSplineIndexNode> nodes;
    nodes.reserve(track_data.records.size());
    for (std::size_t index = 0; index < track_data.records.size(); ++index) {
        const auto& record = track_data.records[index];
        nodes.push_back({
            track_data.next_indices[index],
            std::bit_cast<float>(read_u32_le(
                std::span<const std::byte>(record), 0x04U)),
            std::bit_cast<float>(read_u32_le(
                std::span<const std::byte>(record), 0x08U)),
            std::bit_cast<float>(read_u32_le(
                std::span<const std::byte>(record), 0x0cU)),
            std::bit_cast<float>(read_u32_le(
                std::span<const std::byte>(record), 0x10U)),
            std::bit_cast<float>(read_u32_le(
                std::span<const std::byte>(record), 0x24U)),
            std::bit_cast<float>(read_u32_le(
                std::span<const std::byte>(record), 0x20U)),
        });
    }
    return nodes;
}

TrackSplineInitialSelectionPlan select_initial_track_segment(
    const TrackSplineInitialSelectionObservation& observation) noexcept
{
    TrackSplineInitialSelectionPlan plan;
    if (observation.head_index >= observation.nodes.size()) {
        plan.malformed_link = !observation.nodes.empty();
        return plan;
    }

    const auto first_next = observation.nodes[observation.head_index].next_index;
    if (first_next >= observation.nodes.size()) {
        plan.malformed_link = true;
        return plan;
    }
    if (first_next == observation.head_index) {
        plan.segment_index = observation.head_index;
        plan.returned_empty_head = true;
        return plan;
    }

    auto current = observation.head_index;
    auto best_distance_squared = std::numeric_limits<float>::max();
    bool returned_to_head = false;
    for (std::size_t visited = 0; visited < observation.nodes.size(); ++visited) {
        const auto next = observation.nodes[current].next_index;
        if (next >= observation.nodes.size()) {
            plan.malformed_link = true;
            return plan;
        }
        if (next == observation.head_index) {
            returned_to_head = true;
            break;
        }

        const auto& start = observation.nodes[current];
        const auto& end = observation.nodes[next];
        const auto start_dx = observation.target_xz[0] - start.position_x;
        const auto start_dz = observation.target_xz[1] - start.position_z;
        const auto end_dx = observation.target_xz[0] - end.position_x;
        const auto end_dz = observation.target_xz[1] - end.position_z;
        const auto start_distance_squared = start_dx * start_dx + start_dz * start_dz;
        const auto end_distance_squared = end_dx * end_dx + end_dz * end_dz;
        const auto nearest_distance_squared = start_distance_squared <=
            end_distance_squared ? start_distance_squared : end_distance_squared;
        const auto start_side = start.direction_z * start_dz +
            start.direction_x * start_dx;
        const auto end_side = end.direction_x * end_dx + end.direction_z * end_dz;
        if (start_side > 0.0F && end_side < 0.0F &&
            nearest_distance_squared < best_distance_squared) {
            plan.segment_index = current;
            best_distance_squared = nearest_distance_squared;
        }
        current = next;
    }
    if (plan.segment_index.has_value()) {
        plan.nearest_endpoint_distance_squared = best_distance_squared;
    }
    if (!returned_to_head) {
        plan.malformed_link = true;
    }
    return plan;
}

TrackSplineProjectionPlan project_track_position_to_key(
    const TrackSplineProjectionObservation& observation) noexcept
{
    TrackSplineProjectionPlan plan;
    if (observation.nodes.empty() || observation.head_index >= observation.nodes.size()) {
        plan.malformed_link_or_traversal_guard = !observation.nodes.empty();
        return plan;
    }

    auto current = observation.current_segment;
    auto fraction = observation.current_fraction;
    if (!current.has_value()) {
        const auto initial = select_initial_track_segment({
            observation.nodes, observation.head_index, observation.target_xz});
        if (!initial.segment_index.has_value()) {
            plan.malformed_link_or_traversal_guard = initial.malformed_link;
            return plan;
        }
        current = initial.segment_index;
        fraction = 0.0F;
    }
    if (*current >= observation.nodes.size()) {
        plan.malformed_link_or_traversal_guard = true;
        return plan;
    }

    const auto initial_segment = *current;
    const auto initial_fraction = fraction;
    auto cumulative_key = -observation.nodes[*current].segment_duration * fraction;
    plan.x87_and_pentium_fdiv_rounding_approximated = true;

    for (std::uint32_t iteration = 0; iteration < 10U; ++iteration) {
        const auto next = observation.nodes[*current].next_index;
        if (next >= observation.nodes.size()) {
            plan.segment_index.reset();
            plan.malformed_link_or_traversal_guard = true;
            break;
        }
        const auto& start = observation.nodes[*current];
        const auto& end = observation.nodes[next];
        const auto position_x = (end.position_x - start.position_x) * fraction +
            start.position_x;
        const auto position_z = (end.position_z - start.position_z) * fraction +
            start.position_z;
        const auto direction_x = (end.direction_x - start.direction_x) * fraction +
            start.direction_x;
        const auto direction_z = (end.direction_z - start.direction_z) * fraction +
            start.direction_z;
        const auto direction_length = std::sqrt(
            static_cast<long double>(direction_x) * direction_x +
            static_cast<long double>(direction_z) * direction_z);
        const auto normalized_x = static_cast<float>(direction_x / direction_length);
        const auto normalized_z = static_cast<float>(direction_z / direction_length);
        const auto residual =
            (observation.target_xz[1] - position_z) * normalized_z +
            (observation.target_xz[0] - position_x) * normalized_x;
        plan.iteration_count = iteration + 1U;
        if (std::fabs(residual) < 0.0005F) {
            plan.converged = true;
            break;
        }

        fraction += residual / start.segment_duration;
        auto encoded_fraction = std::bit_cast<std::int32_t>(fraction);
        std::size_t crossed_segments = 0;
        while (encoded_fraction > std::bit_cast<std::int32_t>(1.0F)) {
            cumulative_key += observation.nodes[*current].segment_duration;
            if (observation.maximum_track_key < cumulative_key) {
                current.reset();
                plan.stopped_at_key_limit = true;
                break;
            }
            const auto following = observation.nodes[*current].next_index;
            if (following >= observation.nodes.size() ||
                ++crossed_segments > observation.nodes.size()) {
                current.reset();
                plan.malformed_link_or_traversal_guard = true;
                break;
            }
            current = following;
            fraction -= 1.0F;
            encoded_fraction = std::bit_cast<std::int32_t>(fraction);
        }
        if (!current.has_value()) {
            break;
        }
        if (std::bit_cast<std::uint32_t>(fraction) > 0x80000000U) {
            fraction = 0.0F;
            break;
        }
    }

    if (current.has_value()) {
        cumulative_key += observation.nodes[*current].segment_duration * fraction;
        if (*current == initial_segment && fraction < initial_fraction) {
            current.reset();
            plan.cleared_when_projection_moved_backward = true;
        }
    }
    plan.segment_index = current;
    plan.fraction = fraction;
    plan.projected_track_key = cumulative_key;
    return plan;
}

CarSplinePosePlan plan_car_spline_pose(
    const CarSplinePoseObservation& observation) noexcept
{
    CarSplinePosePlan plan;
    if (!observation.segment_selected) {
        return plan;
    }

    const auto t = observation.segment_fraction;
    const auto t_squared = t * t;
    const auto f1 = ((2.0F * t) - 3.0F) * t_squared + 1.0F;
    const auto f4 = ((-2.0F * t) + 3.0F) * t_squared;
    const auto f3 = ((t - 2.0F) * t + 1.0F) * t;
    const auto f2 = (t - 1.0F) * t_squared;
    const auto& p0 = observation.curve_samples[0];
    const auto& p1 = observation.curve_samples[1];
    const auto& tangent0 = observation.curve_samples[2];
    const auto& tangent1 = observation.curve_samples[3];
    const auto interpolate = [=](const std::size_t axis) {
        return p0[axis] * f1 + p1[axis] * f4 +
            tangent0[axis] * f3 + tangent1[axis] * f2;
    };
    const auto center_x = interpolate(0U);
    const auto center_z = interpolate(1U);

    const auto direction_x = (observation.direction_end[0] -
        observation.direction_start[0]) * t + observation.direction_start[0];
    const auto direction_z = (observation.direction_end[1] -
        observation.direction_start[1]) * t + observation.direction_start[1];
    const auto length = std::sqrt(
        static_cast<long double>(direction_x) * direction_x +
        static_cast<long double>(direction_z) * direction_z);
    const auto normalized_x = static_cast<float>(direction_x / length);
    const auto normalized_z = static_cast<float>(direction_z / length);
    const auto side = (static_cast<std::uint32_t>(observation.placement_ordinal) & 1U)
        ? -4.0F : 4.0F;
    const auto x = center_x + normalized_z * side;
    const auto z = center_z - normalized_x * side;

    plan.transform = {
        normalized_z, 0.0F, -normalized_x,
        0.0F, 1.0F, 0.0F,
        normalized_x, 0.0F, normalized_z,
        x, observation.terrain_height + 0.61F, z,
    };
    plan.helper_succeeded = true;
    plan.x87_and_pentium_fdiv_rounding_approximated = true;
    return plan;
}

CarSplineTrackPlacementPlan plan_car_spline_track_placement(
    const CarSplineTrackPlacementObservation& observation) noexcept
{
    CarSplineTrackPlacementPlan plan;
    plan.key_plan = plan_car_spline_target_key(
        observation.placement_base_key, observation.placement_ordinal);
    plan.segment_plan = select_track_spline_segment({
        observation.geometry_loaded,
        observation.head_index,
        observation.nodes,
        plan.key_plan.target_key,
    });
    if (plan.segment_plan.disposition !=
            TrackSplineSelectionDisposition::selected ||
        !plan.segment_plan.selected_node_index.has_value()) {
        return plan;
    }

    const auto segment_index = *plan.segment_plan.selected_node_index;
    if (segment_index >= observation.nodes.size()) {
        return plan;
    }
    const auto& current = observation.nodes[segment_index];
    if (current.next_index >= observation.nodes.size()) {
        return plan;
    }
    const auto& next = observation.nodes[current.next_index];
    plan.next_node_index = current.next_index;

    CarSplinePoseObservation pose_observation;
    pose_observation.segment_selected = true;
    pose_observation.placement_ordinal = observation.placement_ordinal;
    pose_observation.segment_fraction = plan.segment_plan.segment_fraction;
    pose_observation.curve_samples = {{
        {{current.position_x, current.position_z}},
        {{next.position_x, next.position_z}},
        {{current.direction_x, current.direction_z}},
        {{next.direction_x, next.direction_z}},
    }};
    pose_observation.direction_start = {
        current.direction_x, current.direction_z};
    pose_observation.direction_end = {
        next.direction_x, next.direction_z};
    pose_observation.terrain_height = observation.terrain_height;
    plan.pose_plan = plan_car_spline_pose(pose_observation);
    return plan;
}

CarTrackConstructionPlan plan_car_track_line(
    const CarTrackLineObservation& observation)
{
    CarTrackConstructionPlan plan;
    plan.car_index_before = observation.configured_car_index;
    plan.car_index_after = observation.configured_car_index;
    if (observation.configured_car_index >= observation.configured_car_count) {
        plan.disposition = CarTrackLineDisposition::no_configured_car;
        return plan;
    }

    // The target increments the shared car index before validating the subtype.
    plan.car_index_after = observation.configured_car_index + 1;
    if (observation.model_name.empty()) {
        plan.disposition = CarTrackLineDisposition::bad_car_line;
        return plan;
    }

    TrackCarSubtype subtype;
    switch (observation.raw_subtype) {
    case 0:
        subtype = TrackCarSubtype::pcar;
        plan.record_tag = 0x50434152U; // PCAR
        break;
    case 1:
        subtype = TrackCarSubtype::acar;
        plan.record_tag = 0x41434152U; // ACAR
        plan.calls_acar_helper_0x41a2a0 = true;
        break;
    case 2:
        subtype = TrackCarSubtype::ncar;
        plan.record_tag = 0x4E434152U; // NCAR
        break;
    case 3:
        subtype = TrackCarSubtype::gcar;
        plan.record_tag = 0x47434152U; // GCAR
        plan.increments_gcar_counter = true;
        plan.reads_gcar_timer_value = true;
        break;
    default:
        plan.disposition = CarTrackLineDisposition::unknown_subtype;
        return plan;
    }

    plan.subtype = subtype;
    plan.placement_ordinal = compute_car_placement_ordinal(
        static_cast<std::int32_t>(observation.configured_car_index),
        observation.gcar_count_before,
        observation.raw_subtype == 3U);
    plan.constructor_address = observation.raw_subtype == 3
        ? 0x00466840U
        : 0x00463920U;
    plan.allocation_size = 0x844U;
    if (observation.raw_subtype == 3) {
        plan.gcar_timer_value = observation.gcar_timer_value;
    }
    plan.model_name = observation.model_name;
    plan.creates_configured_ball_effect =
        observation.configured_effect_flag_0xc4;

    bool precomputed_pose_ready = false;
    if (!observation.force_default_placement_0x4dc1b0) {
        if (observation.spline_placement.has_value()) {
            auto placement = *observation.spline_placement;
            placement.placement_ordinal = plan.placement_ordinal;
            plan.spline_placement_plan =
                plan_car_spline_track_placement(placement);
            const auto& pose = plan.spline_placement_plan->pose_plan;
            if (pose.helper_succeeded) {
                plan.placement_route = CarTrackPlacementRoute::precomputed_transform;
                plan.transform = pose.transform;
                plan.precomputed_pose_rounding_approximated =
                    pose.x87_and_pentium_fdiv_rounding_approximated;
                precomputed_pose_ready = true;
            }
        }
        if (!precomputed_pose_ready && observation.precomputed_pose.has_value()) {
            auto pose_observation = *observation.precomputed_pose;
            pose_observation.placement_ordinal = plan.placement_ordinal;
            const auto pose = plan_car_spline_pose(pose_observation);
            if (pose.helper_succeeded) {
                plan.placement_route = CarTrackPlacementRoute::precomputed_transform;
                plan.transform = pose.transform;
                plan.precomputed_pose_rounding_approximated =
                    pose.x87_and_pentium_fdiv_rounding_approximated;
                precomputed_pose_ready = true;
            }
        }
        if (!precomputed_pose_ready && observation.precomputed_transform.has_value()) {
            plan.placement_route = CarTrackPlacementRoute::precomputed_transform;
            plan.transform = *observation.precomputed_transform;
            precomputed_pose_ready = true;
        }
    }
    if (!precomputed_pose_ready) {
        plan.placement_route = CarTrackPlacementRoute::track_basis_and_terrain;
        const auto& basis = observation.track_basis.basis;
        const auto& translation = observation.track_basis.translation;
        const auto x = basis[0] * observation.x +
            basis[6] * observation.z + translation[0];
        const auto z = basis[2] * observation.x +
            basis[8] * observation.z + translation[2];
        plan.transform = {
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
            x, observation.terrain_height + 2.0F, z,
        };
    }
    plan.disposition = CarTrackLineDisposition::construction_ready;
    return plan;
}

bool apply_car_track_placement(
    const std::span<std::byte> runtime_record,
    const CarTrackConstructionPlan& plan) noexcept
{
    if (runtime_record.size() < CarTrackConstructionPlan::runtime_record_size ||
        plan.disposition != CarTrackLineDisposition::construction_ready) {
        return false;
    }

    auto transform_bytes = runtime_record.subspan(
        CarTrackConstructionPlan::transform_record_offset,
        plan.transform.size() * sizeof(float));
    for (std::size_t index = 0; index < plan.transform.size(); ++index) {
        write_u32_le(transform_bytes, index * sizeof(float),
            std::bit_cast<std::uint32_t>(plan.transform[index]));
    }
    return true;
}

std::optional<CarFilePayload> parse_car_file_payload(
    const std::uint32_t resource_version,
    const std::span<const std::byte> payload)
{
    if (resource_version != CarFilePayload::supported_resource_version ||
        payload.size() < CarFilePayload::payload_size) {
        return std::nullopt;
    }

    CarFilePayload result;
    result.resource_version = resource_version;
    result.source_payload_size = payload.size();
    result.payload_size_differs_from_copy_size =
        payload.size() != CarFilePayload::payload_size;
    for (std::size_t index = 0; index < result.raw_words.size(); ++index) {
        result.raw_words[index] = read_u32_le(payload, index * sizeof(std::uint32_t));
    }
    return result;
}

CarFileResourceNames make_car_file_resource_names(std::string_view car_stem)
{
    const auto nul = car_stem.find('\0');
    if (nul != std::string_view::npos) {
        car_stem = car_stem.substr(0, nul);
    }

    CarFileResourceNames result{};
    result.bundle_name.assign(car_stem);
    result.bundle_name.append(CarFilePayload::bundle_extension);
    result.car_file_resource_name.assign(car_stem);
    result.car_file_resource_name.append(CarFilePayload::resource_extension);
    result.upgrade_resource_name.assign(car_stem);
    result.upgrade_resource_name.append(CarFileUpgradePayload::resource_extension);
    return result;
}

CarFileModelNamePlan plan_car_file_model_name(std::string_view resource_path)
{
    const auto nul = resource_path.find('\0');
    if (nul != std::string_view::npos) {
        resource_path = resource_path.substr(0, nul);
    }

    CarFileModelNamePlan result{};
    result.local_copy_bytes = resource_path.size() + 1U;
    result.local_stack_copy_overflow =
        result.local_copy_bytes > CarFileModelNamePlan::local_copy_capacity;

    std::string path_without_extension(resource_path);
    const auto last_dot = path_without_extension.find_last_of('.');
    if (last_dot != std::string::npos) {
        path_without_extension.resize(last_dot);
    }
    const auto last_backslash = path_without_extension.find_last_of('\\');
    const auto basename_start = last_backslash == std::string::npos
        ? 0U : last_backslash + 1U;
    result.normalized_name = path_without_extension.substr(basename_start);
    result.runtime_copy_bytes = result.normalized_name.size() + 1U;
    result.runtime_record_copy_overflow = result.runtime_copy_bytes >
        CarFileModelNamePlan::runtime_record_size -
            CarFileModelNamePlan::runtime_name_offset;
    result.input_complete = !result.local_stack_copy_overflow &&
        !result.runtime_record_copy_overflow;
    return result;
}

bool apply_car_file_model_name(
    const std::span<std::byte> runtime_record,
    const CarFileModelNamePlan& plan) noexcept
{
    if (!plan.input_complete || plan.local_stack_copy_overflow ||
        plan.runtime_record_copy_overflow ||
        runtime_record.size() < CarFileModelNamePlan::runtime_record_size ||
        plan.runtime_copy_bytes != plan.normalized_name.size() + 1U) {
        return false;
    }
    for (std::size_t index = 0; index < plan.normalized_name.size(); ++index) {
        runtime_record[CarFileModelNamePlan::runtime_name_offset + index] =
            static_cast<std::byte>(static_cast<unsigned char>(plan.normalized_name[index]));
    }
    runtime_record[CarFileModelNamePlan::runtime_name_offset +
        plan.normalized_name.size()] = std::byte{0};
    return true;
}

CarFileReadPlan plan_car_file_read(const CarFileReadObservation& observation)
{
    CarFileReadPlan result{};
    result.ordered_steps.push_back(CarFileReadStep::open_resource);
    if (!observation.resource_open_succeeded.has_value()) {
        return result;
    }
    if (!*observation.resource_open_succeeded) {
        result.input_complete = true;
        result.missing_resource_error_called = true;
        result.release_called = false;
        result.ordered_steps.push_back(CarFileReadStep::report_missing_resource);
        return result;
    }

    result.ordered_steps.push_back(CarFileReadStep::find_car_chunk);
    if (!observation.resource_version.has_value()) {
        return result;
    }
    if (*observation.resource_version !=
        CarFilePayload::supported_resource_version) {
        result.input_complete = true;
        result.wrong_version_error_called = true;
        result.ordered_steps.push_back(CarFileReadStep::report_wrong_version);
        result.release_called = true;
        result.ordered_steps.push_back(CarFileReadStep::release_chunk_payload);
        return result;
    }

    result.size_helper_called = true;
    result.ordered_steps.push_back(CarFileReadStep::call_size_check_helper);
    result.fixed_payload_copy_reached = true;
    result.copied_payload_bytes = CarFilePayload::payload_size;
    if (observation.chunk_pointer_returned.has_value()) {
        result.null_chunk_pointer_would_be_used =
            !*observation.chunk_pointer_returned;
    }
    result.ordered_steps.push_back(CarFileReadStep::copy_fixed_payload);
    if (observation.payload_size.has_value()) {
        result.payload_size_differs_from_copy_size =
            *observation.payload_size != CarFilePayload::payload_size;
    } else {
        result.input_complete = false;
    }
    if (observation.upgrade_selector_argument_nonnull.has_value()) {
        result.upgrade_overlay_branch_reached =
            *observation.upgrade_selector_argument_nonnull;
        if (result.upgrade_overlay_branch_reached) {
            result.ordered_steps.push_back(
                CarFileReadStep::enter_upgrade_overlay_branch);
            result.input_complete = false;
        }
    } else {
        result.input_complete = false;
    }

    if (observation.chunk_pointer_returned == false) {
        result.release_called = false;
        result.input_complete = false;
    } else if (observation.chunk_pointer_returned == true) {
        if (observation.upgrade_selector_argument_nonnull == false) {
            result.release_called = true;
            result.ordered_steps.push_back(CarFileReadStep::release_chunk_payload);
            result.input_complete = observation.payload_size.has_value();
        } else {
            result.input_complete = false;
        }
    } else {
        result.input_complete = false;
    }
    return result;
}

CarFileUpgradePayload parse_car_file_upgrade_payload(
    const std::uint32_t resource_version,
    const bool pointer_fixup_enabled,
    const std::span<const std::byte> payload)
{
    constexpr std::size_t relative_data_base = 0x404U;
    constexpr std::size_t set_entries_offset = 0x70U;
    constexpr std::size_t set_count_offset = 0x6cU;
    constexpr std::size_t maximum_pointer_count =
        (relative_data_base - sizeof(std::uint32_t)) /
        sizeof(std::uint32_t);

    CarFileUpgradePayload result{};
    result.resource_version = resource_version;
    result.pointer_fixup_enabled = pointer_fixup_enabled;
    result.version_mismatch = resource_version != 1U;
    if (!pointer_fixup_enabled || payload.size() < relative_data_base) {
        return result;
    }

    const auto set_count = read_u32_le(payload, 0U);
    if (set_count > maximum_pointer_count) {
        return result;
    }

    result.upgrade_sets.reserve(set_count);
    for (std::uint32_t index = 0; index < set_count; ++index) {
        const auto relative_offset = read_u32_le(
            payload, sizeof(std::uint32_t) +
                static_cast<std::size_t>(index) * sizeof(std::uint32_t));
        if (relative_offset > payload.size() - relative_data_base) {
            return result;
        }
        const auto set_offset = relative_data_base + relative_offset;
        if (set_offset > payload.size() - set_entries_offset) {
            return result;
        }

        const auto raw_entry_count = read_u32_le(
            payload, set_offset + set_count_offset);
        const auto entry_count = std::bit_cast<std::int32_t>(raw_entry_count);
        CarFileUpgradeSet set{};
        if (entry_count > 0) {
            const auto unsigned_entry_count =
                static_cast<std::size_t>(entry_count);
            const auto available_bytes =
                payload.size() - (set_offset + set_entries_offset);
            if (unsigned_entry_count > available_bytes /
                    (2U * sizeof(std::uint32_t))) {
                return result;
            }
            set.entries.reserve(unsigned_entry_count);
            for (std::size_t entry_index = 0;
                 entry_index < unsigned_entry_count; ++entry_index) {
                const auto entry_offset = set_offset + set_entries_offset +
                    entry_index * 2U * sizeof(std::uint32_t);
                set.entries.push_back({
                    read_u32_le(payload, entry_offset),
                    read_u32_le(payload, entry_offset + sizeof(std::uint32_t)),
                });
            }
        }
        result.upgrade_sets.push_back(std::move(set));
    }
    result.input_complete = !result.version_mismatch;
    return result;
}

CarFileResourcePayloads parse_car_file_resources(
    const ResourceBundle& bundle,
    const std::string_view car_stem,
    const bool upgrade_pointer_fixup_enabled)
{
    CarFileResourcePayloads result{};
    result.names = make_car_file_resource_names(car_stem);

    if (const auto* car_entry = bundle.find(
            result.names.car_file_resource_name,
            CarFilePayload::resource_type_code)) {
        result.car_file = parse_car_file_payload(
            car_entry->attributes, bundle.payload(*car_entry));
    }
    if (const auto* upgrade_entry = bundle.find(
            result.names.upgrade_resource_name,
            CarFileUpgradePayload::resource_type_code)) {
        result.upgrades = parse_car_file_upgrade_payload(
            upgrade_entry->attributes, upgrade_pointer_fixup_enabled,
            bundle.payload(*upgrade_entry));
    }
    return result;
}

CarFileUpgradePlan plan_car_file_upgrades(
    const CarFileUpgradeObservation& observation)
{
    CarFileUpgradePlan result{};
    if (observation.car_file_version !=
        CarFilePayload::supported_resource_version) {
        result.input_complete = true;
        return result;
    }
    result.car_file_version_supported = true;
    if (!observation.upgrade_resource_found.has_value()) {
        return result;
    }
    if (!*observation.upgrade_resource_found) {
        result.upgrade_resource_missing_panic_called = true;
        return result;
    }
    if (!observation.parsed_upgrade_payload.has_value()) {
        return result;
    }

    const auto& parsed = *observation.parsed_upgrade_payload;
    result.relative_pointer_fixup_branch = parsed.pointer_fixup_enabled;
    result.upgrade_version_panic_called = parsed.resource_version != 1U;
    const auto& upgrade_sets = parsed.upgrade_sets;
    result.input_complete = parsed.input_complete &&
        parsed.resource_version == 1U &&
        observation.selection_flags.size() >= upgrade_sets.size();

    const auto observed_set_count = std::min(
        upgrade_sets.size(), observation.selection_flags.size());
    for (std::size_t set_index = 0;
         set_index < observed_set_count; ++set_index) {
        if (observation.selection_flags[set_index] == 0U) {
            continue;
        }
        const auto& set = upgrade_sets[set_index];
        for (std::size_t entry_index = 0;
             entry_index < set.entries.size(); ++entry_index) {
            const auto& entry = set.entries[entry_index];
            const bool within_payload =
                entry.record_byte_offset <=
                CarFilePayload::payload_size - sizeof(std::uint32_t);
            result.write_outside_car_file_payload |= !within_payload;
            result.ordered_writes.push_back({
                set_index,
                entry_index,
                entry.record_byte_offset,
                entry.value,
                within_payload,
            });
        }
    }
    if (result.write_outside_car_file_payload) {
        result.input_complete = false;
    }
    return result;
}

bool apply_car_file_upgrade_plan(
    const std::span<std::byte> car_file_payload,
    const CarFileUpgradePlan& plan) noexcept
{
    if (!plan.input_complete || !plan.car_file_version_supported ||
        car_file_payload.size() < CarFilePayload::payload_size ||
        plan.write_outside_car_file_payload) {
        return false;
    }
    for (const auto& write : plan.ordered_writes) {
        if (!write.within_car_file_payload) {
            return false;
        }
    }
    for (const auto& write : plan.ordered_writes) {
        write_u32_le(car_file_payload, write.record_byte_offset, write.value);
    }
    return true;
}

std::optional<std::array<std::uint32_t,
    CarFileRuntimeMappingObservation::configuration_word_count>>
extract_car_file_configuration_words(
    const std::span<const std::byte> configured_car_record)
{
    using Observation = CarFileRuntimeMappingObservation;
    if (configured_car_record.size() < Observation::configured_row_size) {
        return std::nullopt;
    }

    std::array<std::uint32_t, Observation::configuration_word_count> words{};
    for (std::size_t index = 0; index < words.size(); ++index) {
        words[index] = read_u32_le(configured_car_record,
            Observation::configuration_word_offset + index * sizeof(std::uint32_t));
    }
    return words;
}

std::optional<CarFileRuntimeMappingObservation>
make_car_file_runtime_mapping_observation(
    const CarFilePayload& car_file,
    const std::span<const std::byte> configured_car_record,
    const bool pentium_fdiv_correction_enabled)
{
    const auto words = extract_car_file_configuration_words(configured_car_record);
    if (!words.has_value()) {
        return std::nullopt;
    }

    CarFileRuntimeMappingObservation observation{};
    observation.car_file = car_file;
    observation.configuration_words = *words;
    observation.pentium_fdiv_correction_enabled =
        pentium_fdiv_correction_enabled;
    return observation;
}

std::optional<CarFileRuntimeMapping> map_car_file_runtime_fields(
    const CarFileRuntimeMappingObservation& observation)
{
    if (observation.car_file.resource_version !=
        CarFilePayload::supported_resource_version) {
        return std::nullopt;
    }

    const auto car_float = [&observation](const std::size_t index) {
        return static_cast<long double>(std::bit_cast<float>(
            observation.car_file.raw_words[index]));
    };
    const auto config_float = [&observation](const std::size_t index) {
        return static_cast<long double>(std::bit_cast<float>(
            observation.configuration_words[index]));
    };
    CarFileRuntimeMapping mapping;
    mapping.writes.reserve(80);
    mapping.pentium_fdiv_correction_approximated =
        observation.pentium_fdiv_correction_enabled;
    const auto write_word = [&mapping](const std::uint16_t offset,
                                       const std::uint32_t word) {
        mapping.writes.push_back({offset, word});
    };
    const auto write_float = [&write_word](const std::uint16_t offset,
                                           const long double value) {
        write_word(offset, std::bit_cast<std::uint32_t>(
            static_cast<float>(value)));
    };
    const auto blend = [&car_float, &config_float](
                           const std::size_t high,
                           const std::size_t low,
                           const std::size_t config) {
        return (car_float(high) - car_float(low)) * config_float(config) +
            car_float(low);
    };

    write_word(0x04, 0x1e0U);
    for (std::size_t index = 0; index < 4; ++index) {
        write_word(static_cast<std::uint16_t>(0x08 + index * 4),
                   observation.car_file.raw_words[index]);
    }
    for (std::size_t index = 0; index < 4; ++index) {
        write_word(static_cast<std::uint16_t>(0x48 + index * 4),
                   observation.car_file.raw_words[index + 4]);
    }
    write_word(0x58, observation.car_file.raw_words[12]);
    write_word(0x5c, observation.car_file.raw_words[13]);
    write_float(0x60, blend(9, 8, 0x10));
    write_float(0x64, blend(11, 10, 0x11));
    write_word(0x68, observation.car_file.raw_words[16]);
    write_word(0x6c, observation.car_file.raw_words[14]);
    write_word(0x70, observation.car_file.raw_words[17]);
    write_word(0x74, observation.car_file.raw_words[15]);

    constexpr long double epsilon = 1.1920929e-7L;
    long double scale = 1.0L;
    for (std::size_t index = 0x18; index <= 0x1d; ++index) {
        const auto value = car_float(index);
        scale *= std::fabs(value) > epsilon ? value : 1.0L;
    }
    const long double numerator = car_float(0x27) + car_float(0x23) +
        car_float(0x1f) + car_float(0x28) + car_float(0x24) +
        car_float(0x20) + car_float(0x29) + car_float(0x25) +
        car_float(0x21) + car_float(0x26) + car_float(0x22) +
        car_float(0x1e);
    const auto ratio = numerator / car_float(0x0e);
    const auto multiplier = config_float(1) * scale * (ratio + 1.0L);
    write_float(0x68, car_float(16) * multiplier);
    write_float(0x6c, car_float(14) * multiplier);

    const long double additional_mass = car_float(0x2b) + car_float(0x30) +
        car_float(0x31) + car_float(0x2d) + car_float(0x2e) +
        car_float(0x2a) + car_float(0x2f) + car_float(0x2c);
    write_float(0x08, car_float(0) + additional_mass);
    write_word(0x78, observation.car_file.raw_words[0x12]);
    write_word(0x7c, observation.car_file.raw_words[0x13]);
    write_word(0x88, observation.car_file.raw_words[0x16]);
    write_float(0x8c, car_float(0x17) * config_float(0x1d));
    write_float(0x90, blend(0x33, 0x32, 0));

    const bool has_car_file_setup = std::fabs(car_float(0x3a)) > epsilon;
    for (std::size_t index = 0; index < 7; ++index) {
        const auto offset = static_cast<std::uint16_t>(0x94 + index * 4);
        write_word(offset, has_car_file_setup
            ? observation.car_file.raw_words[0x3a + index]
            : observation.configuration_words[0x14 + index]);
    }
    write_word(0xb0, has_car_file_setup
        ? observation.car_file.raw_words[0x41]
        : observation.configuration_words[0x1b]);
    const auto setup_count = std::bit_cast<std::int32_t>(
        observation.car_file.raw_words[0x42]);
    if (setup_count < 7) write_word(0xac, 0);
    if (setup_count < 6) write_word(0xa8, 0);
    if (setup_count < 5) write_word(0xa4, 0);

    for (std::size_t index = 0; index < 4; ++index) {
        write_word(static_cast<std::uint16_t>(0xb4 + index * 4),
                   observation.car_file.raw_words[0x34 + index]);
    }
    constexpr std::array<std::size_t, 12> interpolated_config_indices{
        0x0e, 0x0f, 0x04, 0x06, 0x08, 0x0a,
        0x0c, 0x0d, 0x1e, 0x1f, 0x20, 0x21,
    };
    for (std::size_t index = 0; index < interpolated_config_indices.size(); ++index) {
        write_float(static_cast<std::uint16_t>(0xcc + index * 4),
            blend(0x4f + index, 0x43 + index,
                  interpolated_config_indices[index]));
    }
    write_word(0xfc, observation.car_file.raw_words[0x5b]);
    write_word(0x80, observation.car_file.raw_words[0x14]);
    write_word(0x84, observation.car_file.raw_words[0x15]);
    write_word(0xc8, observation.car_file.raw_words[0x39]);
    write_word(0xc4, observation.car_file.raw_words[0x38]);
    for (std::size_t index = 0; index <= 0x06; ++index) {
        write_word(static_cast<std::uint16_t>(0x100 + index * 4),
                   observation.car_file.raw_words[0x5c + index]);
    }
    write_word(0x128, observation.car_file.raw_words[0x66]);
    write_float(0x12c, car_float(0x67) * config_float(0x22));
    for (std::size_t index = 0; index < 4; ++index) {
        write_word(static_cast<std::uint16_t>(0x130 + index * 4),
                   observation.car_file.raw_words[0x68 + index]);
    }
    write_word(0x140, observation.car_file.raw_words[0x6c]);
    write_word(0x144, observation.car_file.raw_words[0x6d]);
    for (std::size_t index = 0; index < 4; ++index) {
        write_word(static_cast<std::uint16_t>(0x150 + index * 4),
                   observation.car_file.raw_words[0x6e + index]);
    }
    write_word(0x160, 0);
    write_float(0x148, blend(0x72, 0x74, 2));
    write_float(0x14c, blend(0x74, 0x73, 2));
    for (std::size_t index = 0; index < 8; ++index) {
        write_word(static_cast<std::uint16_t>(0x164 + index * 4),
                   observation.car_file.raw_words[0x76 + index]);
    }

    auto local_18 = blend(0x82, 0x80, 0x12);
    auto local_14 = blend(0x83, 0x81, 0x13);
    const auto mode = observation.configuration_words[0x1c];
    if (std::fabs(car_float(0x86)) <= epsilon || mode != 1U) {
        if (std::fabs(car_float(0x88)) > epsilon && mode == 2U) {
            local_18 = car_float(0x88);
            local_14 = car_float(0x89);
        }
    }
    else {
        local_18 = car_float(0x86);
        local_14 = car_float(0x87);
    }
    write_float(0x184, car_float(0x7e) + local_18);
    write_float(0x188, car_float(0x7f) + local_14);
    write_float(0x18c, std::fabs(local_18) * car_float(0x84));
    write_float(0x190, std::fabs(local_14) * car_float(0x85));
    return mapping;
}

bool apply_car_file_runtime_mapping(
    const std::span<std::byte> runtime_record,
    const CarFileRuntimeMapping& mapping) noexcept
{
    if (runtime_record.size() < CarFileRuntimeMapping::runtime_record_size) {
        return false;
    }
    for (const auto& write : mapping.writes) {
        if (write.byte_offset > runtime_record.size() ||
            runtime_record.size() - write.byte_offset < sizeof(write.raw_word)) {
            return false;
        }
    }
    for (const auto& write : mapping.writes) {
        for (std::size_t byte_index = 0; byte_index < sizeof(write.raw_word); ++byte_index) {
            runtime_record[write.byte_offset + byte_index] = static_cast<std::byte>(
                (write.raw_word >> (byte_index * 8U)) & 0xffU);
        }
    }
    return true;
}

CarFileRuntimePreparation prepare_car_file_runtime_mapping(
    const CarFileRuntimePreparationObservation& observation)
{
    CarFileRuntimePreparation result{};
    if (observation.resources == nullptr ||
        !observation.resources->car_file.has_value()) {
        return result;
    }

    auto effective_car_file = *observation.resources->car_file;
    if (observation.upgrade_overlay_requested) {
        CarFileUpgradeObservation upgrade_observation{};
        upgrade_observation.car_file_version = effective_car_file.resource_version;
        upgrade_observation.upgrade_resource_found =
            observation.resources->upgrades.has_value();
        upgrade_observation.parsed_upgrade_payload =
            observation.resources->upgrades;
        upgrade_observation.selection_flags = observation.upgrade_selection_flags;
        result.upgrade_plan = plan_car_file_upgrades(upgrade_observation);
        if (!result.upgrade_plan->input_complete) {
            return result;
        }

        std::array<std::byte, CarFilePayload::payload_size> effective_bytes{};
        for (std::size_t index = 0; index < effective_car_file.raw_words.size(); ++index) {
            const auto word = effective_car_file.raw_words[index];
            for (std::size_t byte_index = 0; byte_index < sizeof(word); ++byte_index) {
                effective_bytes[index * sizeof(word) + byte_index] =
                    static_cast<std::byte>((word >> (byte_index * 8U)) & 0xffU);
            }
        }
        if (!apply_car_file_upgrade_plan(effective_bytes, *result.upgrade_plan)) {
            return result;
        }
        auto upgraded = parse_car_file_payload(
            effective_car_file.resource_version, effective_bytes);
        if (!upgraded.has_value()) {
            return result;
        }
        upgraded->source_payload_size = effective_car_file.source_payload_size;
        upgraded->payload_size_differs_from_copy_size =
            effective_car_file.payload_size_differs_from_copy_size;
        effective_car_file = *upgraded;
    }

    CarFileRuntimeMappingObservation mapping_observation{};
    mapping_observation.car_file = effective_car_file;
    mapping_observation.configuration_words = observation.configuration_words;
    mapping_observation.pentium_fdiv_correction_enabled =
        observation.pentium_fdiv_correction_enabled;
    result.mapping = map_car_file_runtime_fields(mapping_observation);
    if (!result.mapping.has_value()) {
        return result;
    }
    result.effective_car_file = effective_car_file;
    result.input_complete = true;
    return result;
}

CarFileRuntimePreparation prepare_car_file_runtime_mapping_from_configured_row(
    const CarFileResourcePayloads& resources,
    const std::span<const std::byte> configured_car_record,
    const std::span<const std::uint8_t> upgrade_selection_flags,
    const bool upgrade_overlay_requested,
    const bool pentium_fdiv_correction_enabled)
{
    const auto words = extract_car_file_configuration_words(configured_car_record);
    if (!words.has_value()) {
        return {};
    }

    CarFileRuntimePreparationObservation observation{};
    observation.resources = &resources;
    observation.configuration_words = *words;
    observation.upgrade_selection_flags = upgrade_selection_flags;
    observation.upgrade_overlay_requested = upgrade_overlay_requested;
    observation.pentium_fdiv_correction_enabled =
        pentium_fdiv_correction_enabled;
    return prepare_car_file_runtime_mapping(observation);
}

CarFileRuntimePreparation prepare_car_file_runtime_mapping_from_setup_payload(
    const CarFileResourcePayloads& resources,
    const std::span<const std::byte> setup_payload,
    const std::span<const std::uint8_t> upgrade_selection_flags,
    const bool upgrade_overlay_requested,
    const bool pentium_fdiv_correction_enabled)
{
    constexpr auto required_size =
        CarFileRuntimeMappingObservation::configuration_word_count *
        sizeof(std::uint32_t);
    if (setup_payload.size() < required_size) {
        return {};
    }

    CarFileRuntimePreparationObservation observation{};
    observation.resources = &resources;
    for (std::size_t index = 0;
         index < observation.configuration_words.size(); ++index) {
        observation.configuration_words[index] = read_u32_le(
            setup_payload, index * sizeof(std::uint32_t));
    }
    observation.upgrade_selection_flags = upgrade_selection_flags;
    observation.upgrade_overlay_requested = upgrade_overlay_requested;
    observation.pentium_fdiv_correction_enabled =
        pentium_fdiv_correction_enabled;
    return prepare_car_file_runtime_mapping(observation);
}

bool apply_car_track_runtime_record(
    const std::span<std::byte> runtime_record,
    const CarFileRuntimePreparation& preparation,
    const CarFileModelNamePlan& model_name,
    const CarTrackConstructionPlan& construction) noexcept
{
    if (runtime_record.size() < CarTrackConstructionPlan::runtime_record_size ||
        !preparation.input_complete || !preparation.mapping.has_value() ||
        !model_name.input_complete || model_name.local_stack_copy_overflow ||
        model_name.runtime_record_copy_overflow ||
        model_name.runtime_copy_bytes != model_name.normalized_name.size() + 1U ||
        construction.disposition != CarTrackLineDisposition::construction_ready ||
        !construction.subtype.has_value() || construction.record_tag == 0U) {
        return false;
    }
    for (const auto& write : preparation.mapping->writes) {
        if (write.byte_offset > runtime_record.size() ||
            runtime_record.size() - write.byte_offset < sizeof(write.raw_word)) {
            return false;
        }
    }
    if (!apply_car_file_model_name(runtime_record, model_name)) {
        return false;
    }
    if (!apply_car_file_runtime_mapping(runtime_record, *preparation.mapping)) {
        return false;
    }

    write_u32_le(runtime_record, 0U, construction.record_tag);
    write_u32_le(runtime_record, sizeof(std::uint32_t),
        CarTrackConstructionPlan::runtime_record_size);
    return apply_car_track_placement(runtime_record, construction);
}

AcarVirtualLookupPlan plan_acar_virtual_lookup(
    const AcarVirtualLookupObservation& observation) noexcept
{
    if (observation.configured_row >= observation.row_counts.size()) {
        return {AcarVirtualLookupDisposition::row_out_of_range, 0U, 0U};
    }

    const auto row = static_cast<std::size_t>(observation.configured_row);
    const auto divisor = std::bit_cast<std::int32_t>(observation.row_counts[row]);
    if (divisor == 0) {
        return {AcarVirtualLookupDisposition::empty_row_divide_by_zero, 0U, 0U};
    }

    // x86 ADD wraps to a DWORD; IDIV then interprets that DWORD as signed.
    const auto numerator_bits = observation.manager_cursor +
        observation.remapped_track_index;
    const auto numerator = std::bit_cast<std::int32_t>(numerator_bits);
    if (numerator == std::numeric_limits<std::int32_t>::min() && divisor == -1) {
        return {AcarVirtualLookupDisposition::signed_divide_overflow, 0U, 0U};
    }
    const auto remainder = numerator % divisor;
    if (remainder < 0 || remainder >= static_cast<std::int32_t>(
            observation.row_entries[row].size())) {
        return {AcarVirtualLookupDisposition::selected_column_out_of_range, 0U, 0U};
    }

    const auto column = static_cast<std::uint32_t>(remainder);
    return {AcarVirtualLookupDisposition::selected, column,
            observation.row_entries[row][column]};
}

AcarTrackAdjustmentPlan plan_acar_track_adjustment(
    const AcarTrackAdjustmentObservation& observation) noexcept
{
    AcarTrackAdjustmentPlan plan;
    plan.queries_world_game_options_0x45c2c0 = true;
    plan.queries_world_track_number_0x45c280 = true;

    constexpr auto hundred_bits = std::bit_cast<std::int32_t>(100.0F);
    constexpr auto one_bits = std::bit_cast<std::int32_t>(1.0F);
    if (std::bit_cast<std::int32_t>(observation.returned_word_0x2c) > hundred_bits) {
        plan.record_word_0x08 = observation.returned_word_0x2c;
    }
    if (std::bit_cast<std::int32_t>(observation.returned_word_0x30) > one_bits) {
        const auto multiplier = std::bit_cast<float>(observation.returned_word_0x30);
        plan.record_word_0x150 = std::bit_cast<std::uint32_t>(
            std::bit_cast<float>(observation.car_record_word_0x150) * multiplier);
        plan.record_word_0x154 = std::bit_cast<std::uint32_t>(
            std::bit_cast<float>(observation.car_record_word_0x154) * multiplier);
    }
    return plan;
}

ConfiguredBallEffectPlan plan_configured_ball_effect(
    const std::uint32_t car_index,
    const float terrain_height) noexcept
{
    ConfiguredBallEffectPlan plan;
    plan.record.terrain_adjusted_y_0x1c = terrain_height + 5000.0F;
    plan.record.car_index_0x34 = car_index;
    return plan;
}

ObstacleModelBoundsResult compute_obstacle_model_bounds(
    const ObstacleModelBoundsObservation& observation) noexcept
{
    if (!observation.vertex_block_present) {
        return {ObstacleModelBounds{}, true, true};
    }

    ObstacleModelBoundsResult result;
    result.bounds = {10000.0F, -10000.0F,
                     10000.0F, -10000.0F,
                     10000.0F, -10000.0F};
    if (observation.vertex_count <= 0) {
        return result;
    }

    const auto count = static_cast<std::size_t>(observation.vertex_count);
    if (observation.vertices.size() < count) {
        result.observation_complete = false;
        return result;
    }

    for (std::size_t index = 0; index < count; ++index) {
        const auto& vertex = observation.vertices[index].words;
        result.bounds.min_x = std::min(result.bounds.min_x, vertex[0]);
        result.bounds.max_x = std::max(result.bounds.max_x, vertex[0]);
        result.bounds.min_y = std::min(result.bounds.min_y, vertex[1]);
        result.bounds.max_y = std::max(result.bounds.max_y, vertex[1]);
        result.bounds.min_z = std::min(result.bounds.min_z, vertex[2]);
        result.bounds.max_z = std::max(result.bounds.max_z, vertex[2]);
    }
    return result;
}

std::optional<CheckpointConstructionPlan> plan_checkpoint_track_line(
    const CheckpointTrackLineObservation& observation)
{
    if (observation.scanf_conversion_count != 5 || observation.name.empty()) {
        return std::nullopt;
    }

    CheckpointConstructionPlan plan;
    plan.name = observation.name;
    plan.record.start = {
        observation.start_x,
        observation.start_terrain_height,
        observation.start_z,
    };
    plan.record.end = {
        observation.end_x,
        observation.end_terrain_height,
        observation.end_z,
    };
    plan.route = observation.track_state_selector_0x53b194 != 0U
        ? CheckpointConstructionRoute::visual_scene_object
        : CheckpointConstructionRoute::serialized_object_record;
    return plan;
}

std::optional<WobbleTrackRecord> make_wobble_track_record(
    const WobbleTrackLineObservation& observation) noexcept
{
    if (observation.scanf_conversion_count != 2) {
        return std::nullopt;
    }

    WobbleType type;
    if (equals_ascii_case_insensitive(observation.type_name, "pole")) {
        type = WobbleType::pole;
    }
    else if (equals_ascii_case_insensitive(observation.type_name, "flap")) {
        type = WobbleType::flap;
    }
    else {
        return std::nullopt;
    }

    return WobbleTrackRecord{
        0x574F424CU,
        0x20U,
        10000.0F,
        12500.0F,
        12500.0F,
        12500.0F,
        observation.parsed_parameter,
        type,
    };
}

StaticTrackLinePlan plan_static_track_line(
    const StaticTrackLineObservation& observation) noexcept
{
    if (!observation.parser_enabled_by_dispatch) {
        return {StaticTrackLineDisposition::skipped_by_dispatch_gate, std::nullopt};
    }
    if (observation.scanf_conversion_count != 10) {
        return {StaticTrackLineDisposition::malformed_record, std::nullopt};
    }
    if (!equals_ascii_case_insensitive(observation.type_name, "box")) {
        return {StaticTrackLineDisposition::unknown_type, std::nullopt};
    }

    StaticTrackRecord record;
    record.payload.parsed_vector_0x2c = observation.parsed_vector_0x2c;
    record.payload.raw_word_0x38 = 0;
    record.payload.parsed_vector_0x3c = observation.parsed_vector_0x3c;

    const auto& rotation = observation.rotation_vector;
    const long double angle = std::sqrt(
        static_cast<long double>(rotation[0]) * rotation[0] +
        static_cast<long double>(rotation[1]) * rotation[1] +
        static_cast<long double>(rotation[2]) * rotation[2]);
    auto& matrix = record.payload.rotation_matrix_0x08;
    if (angle <= 0.0001L) {
        matrix = {1.0F, 0.0F, 0.0F,
                  0.0F, 1.0F, 0.0F,
                  0.0F, 0.0F, 1.0F};
    }
    else {
        const auto x = static_cast<long double>(rotation[0]) / angle;
        const auto y = static_cast<long double>(rotation[1]) / angle;
        const auto z = static_cast<long double>(rotation[2]) / angle;
        const auto sine = std::sin(angle);
        const auto cosine = std::cos(angle);
        const auto one_minus_cosine = 1.0L - cosine;

        const float xy_term = static_cast<float>(one_minus_cosine * y * x);
        const float z_sine_term = static_cast<float>(sine * z);
        const float yz_term = static_cast<float>(one_minus_cosine * z);
        const float xz_term = static_cast<float>(one_minus_cosine * z * x);
        const float y_sine_term = static_cast<float>(sine * y);

        matrix = {
            static_cast<float>(x * x * one_minus_cosine + cosine),
            static_cast<float>(sine * z + xy_term),
            static_cast<float>(xz_term - y_sine_term),
            xy_term - z_sine_term,
            static_cast<float>(y * y * one_minus_cosine + cosine),
            static_cast<float>(y * yz_term + sine * x),
            xz_term + y_sine_term,
            static_cast<float>(y * yz_term - sine * x),
            static_cast<float>(z * z * one_minus_cosine + cosine),
        };
    }

    return {StaticTrackLineDisposition::record_ready, record};
}

ObstacleTrackLinePlan plan_obstacle_track_line(
    const ObstacleTrackLineObservation& observation)
{
    if (observation.scanf_conversion_count != 6) {
        return {ObstacleLineDisposition::malformed_record, std::nullopt};
    }

    ObstacleType type;
    if (equals_ascii_case_insensitive(observation.type_name, "ball")) {
        type = ObstacleType::ball;
    }
    else if (equals_ascii_case_insensitive(observation.type_name, "cube")) {
        type = ObstacleType::cube;
    }
    else if (equals_ascii_case_insensitive(observation.type_name, "prism")) {
        type = ObstacleType::prism;
    }
    else {
        return {ObstacleLineDisposition::unknown_type, std::nullopt};
    }

    const float extent_x = observation.model_bounds.max_x -
        observation.model_bounds.min_x;
    const float extent_y = observation.model_bounds.max_y -
        observation.model_bounds.min_y;
    const float extent_z = observation.model_bounds.max_z -
        observation.model_bounds.min_z;
    constexpr float target_extent_factor = 10.749798F;

    ObstacleTrackRecord record;
    record.scale_0x08 = observation.scale;
    record.scaled_extent_term_y_0x0c =
        extent_y * extent_y * target_extent_factor * observation.scale;
    record.scaled_extent_term_z_0x10 =
        extent_z * extent_z * target_extent_factor * observation.scale;
    record.scaled_extent_term_x_0x14 =
        extent_x * extent_x * target_extent_factor * observation.scale;
    record.x_0x18 = observation.x;
    record.adjusted_y_0x1c =
        (observation.terrain_height - observation.model_bounds.min_y) + 4.0F;
    record.z_0x20 = observation.z;
    record.parsed_parameter_0x24 = observation.raw_parameter_0x14;
    record.extent_x_0x28 = extent_x;
    record.extent_y_0x2c = extent_y;
    record.extent_z_0x30 = extent_z;
    record.type_0x34 = type;

    return {
        ObstacleLineDisposition::record_ready,
        ObstacleTrackConstruction{record, std::string(observation.asset_name)},
    };
}

} // namespace viper
