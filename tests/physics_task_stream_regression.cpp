#include "viper/physics_task_stream.hpp"
#include "viper/physics_task_factory.hpp"
#include "viper/world_track_objects.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

viper::TrackObjectTextTable make_track_object_table(
    const std::initializer_list<std::string_view> lines)
{
    viper::TrackObjectTextTable table{};
    table.line_count = static_cast<std::uint32_t>(lines.size());
    table.line_stride = 64U;
    table.line_offsets.fill(0U);
    table.raw_payload.resize(viper::TrackObjectTextTable::header_size +
        lines.size() * table.line_stride);
    std::size_t index = 0U;
    for (const auto line : lines) {
        const auto offset = viper::TrackObjectTextTable::header_size +
            index * table.line_stride;
        const auto length = std::min(line.size(),
            static_cast<std::size_t>(table.line_stride - 1U));
        std::memcpy(table.raw_payload.data() +
            static_cast<std::ptrdiff_t>(offset), line.data(), length);
        ++index;
    }
    return table;
}

std::vector<std::byte> make_physics_record(
    const std::uint32_t tag, const std::size_t size)
{
    std::vector<std::byte> bytes(size);
    const auto write_u32 = [&bytes](const std::size_t offset,
                                    const std::uint32_t value) {
        for (std::size_t index = 0U; index < 4U; ++index) {
            bytes[offset + index] = static_cast<std::byte>(
                (value >> (index * 8U)) & 0xFFU);
        }
    };
    write_u32(0U, tag);
    write_u32(4U, static_cast<std::uint32_t>(size));
    for (std::size_t index = 8U; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>(index & 0xFFU);
    }
    return bytes;
}

std::uint32_t read_u32_le(const std::span<const std::byte> bytes,
                          const std::size_t offset)
{
    return std::to_integer<std::uint32_t>(bytes[offset])
        | (std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

} // namespace

int main()
{
    try {
        viper::PhysicsAcarRuntimeInitializationObservation ai_init{};
        ai_init.linked_object_value_0x18 = 2.0F;
        ai_init.last_model_node_value_0x18 = 4.0F;
        ai_init.last_model_node_value_0x20 = 12.0F;
        ai_init.last_model_node_value_0x38 = 1.0F;
        const auto ai_plan = viper::plan_physics_acar_runtime_initialization(ai_init);
        require(ai_plan.derived_threshold_0x1044 &&
                *ai_plan.derived_threshold_0x1044 == 5.0F,
            "ACAR threshold must divide by the mean of both node values");
        const std::array<viper::PhysicsAcarModelNodeObservation, 2> threshold_nodes{{
            {1U, 2U, 0.0F, 2.0F, 0.0F, 0.0F},
            {2U, 1U, 0.0F, 4.0F, 12.0F, 1.0F}}};
        const auto threshold = viper::plan_acar_line_threshold_candidate(
            threshold_nodes, std::bit_cast<std::uint32_t>(2.5F));
        require(threshold && threshold->derived_threshold_0x1044 == 5.0F &&
                threshold->normalized_value_0x1030 == 0.5F && !threshold->derived_value_selected,
            "AI line threshold must use the last node and its linked node");
        const auto replaced = viper::plan_acar_line_threshold_candidate(
            threshold_nodes, std::bit_cast<std::uint32_t>(-1.0F));
        require(replaced && replaced->derived_value_selected &&
                replaced->selected_profile_value_0x1028 == std::bit_cast<std::uint32_t>(5.0F) &&
                replaced->normalized_value_0x1030 == 1.0F,
            "negative profile words must select the descriptor-derived threshold");
        viper::PhysicsAcarDerivedRuntimeState acar_derived{};
        viper::initialize_acar_derived_constructor_prefix(acar_derived);
        for (std::size_t i = 0; i < acar_derived.bytes.size(); ++i) {
            const auto offset = i + acar_derived.base_offset;
            const bool cleared = offset >= 0x1054U && offset < 0x1070U;
            require((acar_derived.initialized_bytes[i] != 0U) == cleared,
                "ACAR derived prefix must mark only its seven cleared words");
            if (cleared) require(acar_derived.bytes[i] == std::byte{},
                "ACAR derived prefix must clear all seven words");
        }
        viper::set_acar_derived_profile_word(acar_derived, 0x1234abcdU);
        constexpr auto profile_offset = 0xf50U - viper::PhysicsAcarDerivedRuntimeState::base_offset;
        require(read_u32_le(acar_derived.bytes, profile_offset) == 0x1234abcdU &&
                acar_derived.initialized_bytes[profile_offset] &&
                !acar_derived.initialized_bytes[profile_offset - 1U] &&
                !acar_derived.initialized_bytes[profile_offset + 4U],
            "ACAR profile word must not initialize adjacent unknown fields");
        viper::IdealLineTrackData track_curve{};
        track_curve.records.resize(2U);
        track_curve.next_indices = {1U, 0U};
        const auto write_curve_float = [&track_curve](const std::size_t row,
            const std::size_t offset, const float value) {
            const auto bits = std::bit_cast<std::uint32_t>(value);
            for (std::size_t i = 0; i < 4U; ++i)
                track_curve.records[row][offset + i] =
                    static_cast<std::byte>((bits >> (8U * i)) & 0xffU);
        };
        write_curve_float(0U, 0x04U, 10.0F);
        write_curve_float(0U, 0x08U, -3.0F);
        write_curve_float(0U, 0x0cU, 2.0F);
        write_curve_float(0U, 0x10U, 4.0F);
        write_curve_float(0U, 0x18U, 6.0F);
        write_curve_float(0U, 0x1cU, 0.025F);
        write_curve_float(0U, 0x20U, 5.0F);
        write_curve_float(0U, 0x38U, 8.0F);
        write_curve_float(0U, 0x3cU, 7.0F);
        track_curve.records[0][0x2aU] = std::byte{3};
        const auto curve_nodes = viper::make_track_curve_nodes(track_curve);
        const auto model_nodes = viper::make_acar_model_node_observations(track_curve);
        require(model_nodes && model_nodes->size() == 2U &&
                (*model_nodes)[0].address == 1U && (*model_nodes)[0].next_address == 2U &&
                (*model_nodes)[1].next_address == 1U &&
                (*model_nodes)[0].scalar_at_0x1c == 0.025F &&
                (*model_nodes)[0].value_0x18 == 6.0F &&
                (*model_nodes)[0].value_0x20 == 5.0F &&
                (*model_nodes)[0].value_0x38 == 8.0F,
            "AI line nodes must preserve descriptor and threshold source values");
        viper::PhysicsAcarTaskConstructorObservation descriptor_input{};
        descriptor_input.model_data_present = true;
        descriptor_input.model_data_object_address = 0x1000U;
        descriptor_input.model_descriptor_count = 2U;
        descriptor_input.model_descriptor_allocation_succeeded = true;
        descriptor_input.helper_0x41b020_nonzero = true;
        descriptor_input.model_nodes = *model_nodes;
        const auto descriptors = viper::plan_physics_acar_task_constructor(descriptor_input);
        const auto descriptor_geometry = viper::make_acar_descriptor_geometry(*model_nodes);
        require(!descriptors.model_helper_input_capture_incomplete &&
                descriptors.model_descriptors.size() == 2U &&
                descriptors.model_descriptors[0].source_node_address == 1U &&
                descriptors.model_descriptors[1].source_node_address == 2U &&
                descriptor_geometry.size() == 2U &&
                descriptor_geometry[0].mapped_scalar == descriptors.model_descriptors[0].mapped_scalar &&
                descriptor_geometry[1].mapped_scalar == descriptors.model_descriptors[1].mapped_scalar,
            "AI line node captures must feed the recovered descriptor constructor");
        require(curve_nodes && curve_nodes->nodes.size() == 2U &&
                curve_nodes->nodes[0].address == 1U &&
                curve_nodes->nodes[0].next_address == 2U &&
                curve_nodes->nodes[1].next_address == 1U &&
                curve_nodes->nodes[0].position_x_0x04 == 10.0F &&
                curve_nodes->nodes[0].position_y_0x08 == -3.0F &&
                curve_nodes->nodes[0].tangent_x_0x0c == 2.0F &&
                curve_nodes->nodes[0].tangent_y_0x10 == 4.0F &&
                curve_nodes->nodes[0].update_denominator_0x20 == 5.0F &&
                curve_nodes->nodes[0].route_scalar_0x3c == 7.0F &&
                curve_nodes->category_labels[0] == 3U,
            "track rows must preserve spline fields, category and circular links");
        track_curve.next_indices[1] = 2U;
        require(!viper::make_track_curve_nodes(track_curve),
            "track curve adapter must reject an out-of-range link");
        require(!viper::make_acar_model_node_observations(track_curve),
            "AI line node adapter must reject an out-of-range link");
        viper::TrackCurveNodes straight_curve{};
        straight_curve.nodes = {
            {1U, 2U, 0.0F, 0.0F, 10.0F, 0.0F, 10.0F, 0.0F},
            {2U, 3U, 10.0F, 0.0F, 10.0F, 0.0F, 10.0F, 0.0F},
            {3U, 1U, 20.0F, 0.0F, 10.0F, 0.0F, 20.0F, 0.0F}};
        straight_curve.category_labels = {1U, 2U, 3U};
        viper::TrackPositionIteratorState tracker{};
        require(viper::initialize_track_position_iterator(tracker, straight_curve,
                    5.0F, 0.0F) == viper::TrackPositionIteratorStatus::updated &&
                tracker.node == 1U && tracker.reset_pending,
            "track iterator must seek the initial physical position");
        const auto first_position = viper::update_track_position_iterator(
            tracker, straight_curve, 5.0F, 0.0F);
        require(first_position.status == viper::TrackPositionIteratorStatus::updated &&
                !tracker.reset_pending && !first_position.category_changed &&
                first_position.current_category == 1U,
            "first position update must perform the reset seek");
        const auto next_position = viper::update_track_position_iterator(
            tracker, straight_curve, 6.0F, 0.0F);
        require(next_position.status == viper::TrackPositionIteratorStatus::updated &&
                tracker.node == 1U && tracker.curve_valid &&
                tracker.fraction > 0.5F && !next_position.category_changed,
            "subsequent physical positions must advance the tracked fraction");
        const auto crossed_position = viper::update_track_position_iterator(
            tracker, straight_curve, 10.5F, 0.0F);
        require(crossed_position.status == viper::TrackPositionIteratorStatus::updated &&
                tracker.node == 2U && crossed_position.previous_category == 1U &&
                crossed_position.current_category == 2U && crossed_position.category_changed,
            "physical movement across a segment must report the category transition");
        auto reset_tracker = tracker;
        reset_tracker.node = 2U;
        reset_tracker.reset_pending = true;
        const auto changed_category = viper::update_track_position_iterator(
            reset_tracker, straight_curve, 5.0F, 0.0F);
        require(changed_category.status == viper::TrackPositionIteratorStatus::updated &&
                changed_category.previous_category == 2U &&
                changed_category.current_category == 1U &&
                changed_category.category_changed,
            "reset seek must report the old and new category even when decreasing");
        auto invalid_tracker = tracker;
        invalid_tracker.node = 99U;
        const auto missing_node = viper::update_track_position_iterator(
            invalid_tracker, straight_curve, 6.0F, 0.0F);
        require(missing_node.status == viper::TrackPositionIteratorStatus::missing_node &&
                invalid_tracker.node == 99U,
            "missing iterator nodes must not modify tracker state");
        const auto default_track_service = viper::plan_physics_track_service(
            0U, true, true, true);
        const auto alternate_track_service = viper::plan_physics_track_service(
            1U, true, true, true);
        const auto alternate_track_service_three = viper::plan_physics_track_service(
            3U, true, true, true);
        const auto auxiliary_track_service = viper::plan_physics_track_service(
            5U, true, true, true);
        require(default_track_service.vtable_address == 0x004c84d8U &&
                    default_track_service.checkpoint_virtual_0x34_target ==
                        0x0043cb10U &&
                    alternate_track_service.vtable_address == 0x004c7810U &&
                    alternate_track_service.checkpoint_virtual_0x34_target ==
                        0x0043be80U &&
                    alternate_track_service_three.vtable_address ==
                        0x004c7880U &&
                    alternate_track_service_three.checkpoint_virtual_0x34_target ==
                        0x0043be80U &&
                    auxiliary_track_service.vtable_address == 0x004c8420U &&
                    auxiliary_track_service.checkpoint_virtual_0x34_target ==
                        0x0043c3b0U,
                "track service factory lost its recovered checkpoint callback targets");

        const auto default_checkpoint_dispatch =
            viper::plan_physics_track_checkpoint_dispatch({0U, true,
                0x12345678U, 2U, false});
        require(default_checkpoint_dispatch.status ==
                    viper::PhysicsTrackCheckpointDispatchStatus::dispatched &&
                    default_checkpoint_dispatch.callback_address == 0x0043cb10U &&
                    default_checkpoint_dispatch.service_count_after == 3U &&
                    default_checkpoint_dispatch.object_writes.size() == 1U &&
                    default_checkpoint_dispatch.object_writes[0].byte_offset == 0x34U &&
                    default_checkpoint_dispatch.object_writes[0].value == 3U &&
                    !default_checkpoint_dispatch.checkpoint_pointer_stored,
                "selector-0 checkpoint callback must only increment service +0x34");

        const auto alternate_checkpoint_first =
            viper::plan_physics_track_checkpoint_dispatch({1U, true,
                0x12345678U, 0U, false});
        const auto alternate_checkpoint_second =
            viper::plan_physics_track_checkpoint_dispatch({1U, true,
                0x56781234U, 1U, false});
        const auto alternate_checkpoint_third =
            viper::plan_physics_track_checkpoint_dispatch({3U, true,
                0x87654321U, 2U, false});
        require(alternate_checkpoint_first.status ==
                    viper::PhysicsTrackCheckpointDispatchStatus::dispatched &&
                    alternate_checkpoint_first.callback_address == 0x0043be80U &&
                    alternate_checkpoint_first.checkpoint_pointer_stored &&
                    alternate_checkpoint_first.object_writes.size() == 2U &&
                    alternate_checkpoint_first.object_writes[0].byte_offset == 0x44U &&
                    alternate_checkpoint_first.object_writes[0].value == 0x12345678U &&
                    alternate_checkpoint_first.object_writes[1].byte_offset == 0x50U &&
                    alternate_checkpoint_first.object_writes[1].value == 1U &&
                    alternate_checkpoint_second.checkpoint_pointer_stored &&
                    alternate_checkpoint_second.object_writes[0].byte_offset == 0x48U &&
                    alternate_checkpoint_second.object_writes[0].value == 0x56781234U &&
                    alternate_checkpoint_second.object_writes[1].byte_offset == 0x50U &&
                    alternate_checkpoint_second.object_writes[1].value == 2U &&
                    alternate_checkpoint_third.checkpoint_pointer_stored &&
                    alternate_checkpoint_third.object_writes[0].byte_offset == 0x4cU &&
                    alternate_checkpoint_third.object_writes[0].value == 0x87654321U &&
                    alternate_checkpoint_third.object_writes[1].byte_offset == 0x50U &&
                    alternate_checkpoint_third.object_writes[1].value == 3U,
                "selector-1/3 checkpoint callback must retain three ordered pointers");

        const auto checkpoint_overflow =
            viper::plan_physics_track_checkpoint_dispatch({1U, true,
                0xabcdef01U, 3U, true});
        const auto checkpoint_overflow_fault =
            viper::plan_physics_track_checkpoint_dispatch({3U, true,
                0xabcdef01U, 3U, false});
        require(checkpoint_overflow.status ==
                    viper::PhysicsTrackCheckpointDispatchStatus::dispatched &&
                    checkpoint_overflow.formatted_reporter_called &&
                    checkpoint_overflow.formatted_reporter_address == 0x00410ea0U &&
                    checkpoint_overflow.format_string_address == 0x004d5b78U &&
                    checkpoint_overflow.object_writes.size() == 1U &&
                    checkpoint_overflow.object_writes[0].byte_offset == 0x50U &&
                    checkpoint_overflow.object_writes[0].value == 4U &&
                    checkpoint_overflow_fault.status ==
                    viper::PhysicsTrackCheckpointDispatchStatus::
                        formatted_reporter_did_not_return &&
                    checkpoint_overflow_fault.object_writes.empty() &&
                    !checkpoint_overflow_fault.service_count_after.has_value(),
                "checkpoint pointer overflow must follow the observed report-then-increment path");

        const auto auxiliary_checkpoint_dispatch =
            viper::plan_physics_track_checkpoint_dispatch({5U, true,
                0x12345678U, 7U, false});
        const auto null_checkpoint_dispatch =
            viper::plan_physics_track_checkpoint_dispatch({0U, false,
                0x12345678U, 7U, false});
        const auto unknown_checkpoint_dispatch =
            viper::plan_physics_track_checkpoint_dispatch({2U, true,
                0x12345678U, 0U, false});
        require(auxiliary_checkpoint_dispatch.status ==
                    viper::PhysicsTrackCheckpointDispatchStatus::dispatched &&
                    auxiliary_checkpoint_dispatch.callback_address == 0x0043c3b0U &&
                    auxiliary_checkpoint_dispatch.object_writes.empty() &&
                    !auxiliary_checkpoint_dispatch.service_count_after.has_value() &&
                    null_checkpoint_dispatch.status ==
                    viper::PhysicsTrackCheckpointDispatchStatus::null_receiver_fault &&
                    !null_checkpoint_dispatch.virtual_0x34_called &&
                    unknown_checkpoint_dispatch.status ==
                    viper::PhysicsTrackCheckpointDispatchStatus::unsupported_selector &&
                    !unknown_checkpoint_dispatch.virtual_0x34_called,
                "null, unsupported, or selector-5 checkpoint dispatch was misclassified");

        constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 10> sizes{{
            {0x00434152U, 0x18CU}, // RAC\0
            {0x47434152U, 0x190U}, // GCAR
            {0x50434152U, 0x18CU}, // PCAR
            {0x41434152U, 0x18CU}, // ACAR
            {0x4E434152U, 0x18CU}, // NCAR
            {0x53544154U, 0U},     // STAT
            {0x4F425354U, 0x34U},  // OBST
            {0x574F424CU, 0x34U},  // WOBL
            {0x43484B50U, 0x34U},  // CHKP
            {0x42414C4CU, 0x34U},  // BALL
        }};
        for (const auto [tag, expected] : sizes) {
            const auto actual = viper::physics_task_stream_advance_for_tag(tag);
            require(actual.has_value() && *actual == expected,
                    "resolver tag advance differs from recovered table");
        }
        require(!viper::physics_task_stream_advance_for_tag(0x12345678U),
                "unknown record tag was assigned a cursor advance");

        constexpr std::array<std::uint32_t, 5> tags{
            0x50434152U, 0x53544154U, 0x4F425354U, 0x47434152U, 0x42414C4CU};
        const auto plan = viper::plan_physics_task_stream_offsets(tags);
        require(plan.status == viper::PhysicsTaskStreamPlanStatus::complete &&
                    plan.initial_cursor == 0U && plan.records.size() == tags.size() &&
                    plan.final_cursor == 0x384U,
                "default-zero stream cursor fold is incorrect");
        require(plan.records[0].stream_offset == 0U &&
                    plan.records[1].stream_offset == 0x18CU &&
                    plan.records[2].stream_offset == 0x18CU &&
                    plan.records[3].stream_offset == 0x1C0U &&
                    plan.records[4].stream_offset == 0x350U,
                "record offsets do not follow native class-size advances");

        auto car_record = make_physics_record(0x50434152U, 0x1E4U);
        auto static_record = make_physics_record(0x53544154U, 0x48U);
        auto obstacle_record = make_physics_record(0x4F425354U, 0x38U);
        const std::array<viper::PhysicsTaskRecordInput, 3> input_records{{
            {car_record}, {static_record}, {obstacle_record}}};
        const auto serialized = viper::serialize_physics_task_records(
            input_records, 0x100U);
        constexpr std::size_t static_entry = 4U + 0x1E4U;
        constexpr std::size_t obstacle_entry = static_entry + 4U + 0x48U;
        require(serialized.status ==
                    viper::PhysicsTaskRecordSerializationStatus::complete &&
                    serialized.records.size() == 3U &&
                    serialized.records[0].cursor_offset == 0x100U &&
                    serialized.records[1].cursor_offset == 0x28CU &&
                    serialized.records[2].cursor_offset == 0x28CU &&
                    serialized.records[2].cursor_advance == 0x34U &&
                    serialized.final_cursor == 0x2C0U,
                "record serializer must use native cursor advances, not payload sizes");
        require(read_u32_le(serialized.bytes, 0U) == 0x100U &&
                    read_u32_le(serialized.bytes, 4U) == 0x50434152U &&
                    read_u32_le(serialized.bytes, static_entry) == 0x28CU &&
                    read_u32_le(serialized.bytes, static_entry + 4U) == 0x53544154U &&
                    read_u32_le(serialized.bytes, obstacle_entry) == 0x28CU &&
                    read_u32_le(serialized.bytes, obstacle_entry + 4U) == 0x4F425354U &&
                    serialized.bytes.size() == obstacle_entry + 4U + 0x38U &&
                    std::equal(car_record.begin(), car_record.end(),
                        serialized.bytes.begin() + 4),
                "serializer must frame each complete record with a little-endian cursor");

        viper::PhysicsTaskRecordStreamSession session(0x100U);
        const auto session_first = session.append_record(car_record);
        require(session_first.status ==
                    viper::PhysicsTaskRecordSerializationStatus::complete &&
                    session_first.records.size() == 1U &&
                    session_first.records[0].cursor_offset == 0x100U &&
                    session.final_cursor() == 0x28CU,
                "session stream must append a record at its current explicit cursor");
        const auto session_before_reject = session.snapshot();
        const auto session_unknown_record = make_physics_record(0x12345678U, 8U);
        const auto session_reject = session.append_record(session_unknown_record);
        require(session_reject.status ==
                    viper::PhysicsTaskRecordSerializationStatus::unknown_record_tag &&
                    session_reject.failed_record_index == 1U &&
                    session.final_cursor() == session_before_reject.final_cursor &&
                    session.snapshot().bytes == session_before_reject.bytes,
                "rejected session append must leave accumulated bytes and cursor unchanged");
        const auto session_second = session.append_record(static_record);
        const auto session_snapshot = session.snapshot();
        const auto session_decoded =
            viper::read_physics_task_record_stream(session_snapshot.bytes);
        require(session_second.status ==
                    viper::PhysicsTaskRecordSerializationStatus::complete &&
                    session_snapshot.records.size() == 2U &&
                    session_snapshot.final_cursor == 0x28CU &&
                    session_decoded.status ==
                        viper::PhysicsTaskRecordStreamReadStatus::complete &&
                    session_decoded.records.size() == 2U &&
                    session_decoded.records[1].record_tag == 0x53544154U &&
                    session_decoded.records[1].cursor_offset == 0x28CU,
                "session snapshot must preserve appended record order and cursor framing");

        const auto decoded = viper::read_physics_task_record_stream(serialized.bytes);
        require(decoded.status == viper::PhysicsTaskRecordStreamReadStatus::complete &&
                    decoded.records.size() == 3U && decoded.consumed_bytes ==
                        serialized.bytes.size() && decoded.final_cursor == 0x2C0U &&
                    decoded.records[0].record_tag == 0x50434152U &&
                    decoded.records[0].cursor_offset == 0x100U &&
                    decoded.records[0].record_size == 0x1E4U &&
                    decoded.records[0].record_bytes_offset == 4U &&
                    decoded.records[1].cursor_offset == 0x28CU &&
                    decoded.records[2].cursor_offset == 0x28CU,
                "reader must recover serialized records and native cursor positions");
        const auto object_inventory = viper::inventory_physics_task_objects(
            decoded, serialized.bytes);
        require(object_inventory.status ==
                    viper::PhysicsTaskObjectInventoryStatus::complete &&
                    object_inventory.entries.size() == decoded.records.size() &&
                    object_inventory.entries[0].stream_record_index == 0U &&
                    object_inventory.entries[0].record_tag == 0x50434152U &&
                    object_inventory.entries[0].stream_cursor == 0x100U &&
                    object_inventory.entries[0].record_size == 0x1E4U &&
                    object_inventory.entries[0].allocation_bytes == 0x0EF0U &&
                    object_inventory.entries[0].constructor_address == 0x004381E0U &&
                    !object_inventory.entries[0].native_object_materialized &&
                    !object_inventory.entries[0].fixed_step_callback_supported,
                "ordered task inventory must retain record identity and factory metadata without claiming construction");
        auto truncated_decoded = decoded;
        truncated_decoded.records[1].record_bytes_offset = serialized.bytes.size();
        const auto invalid_inventory = viper::inventory_physics_task_objects(
            truncated_decoded, serialized.bytes);
        require(invalid_inventory.status ==
                    viper::PhysicsTaskObjectInventoryStatus::record_out_of_bounds &&
                    invalid_inventory.failed_record_index == 1U &&
                    invalid_inventory.entries.empty(),
                "task inventory must reject malformed payload spans atomically");
        auto mismatched_inventory_bytes = serialized.bytes;
        mismatched_inventory_bytes[0] ^= std::byte{1U};
        const auto mismatched_inventory = viper::inventory_physics_task_objects(
            decoded, mismatched_inventory_bytes);
        require(mismatched_inventory.status ==
                    viper::PhysicsTaskObjectInventoryStatus::decoded_record_mismatch &&
                    mismatched_inventory.failed_record_index == 0U &&
                    mismatched_inventory.entries.empty(),
                "task inventory must reject a same-size byte buffer whose stream cursor differs from decoded identity");
        const auto pcar_binding = viper::bind_player_pcar_task_record(
            decoded, serialized.bytes, car_record, 0x100U);
        require(pcar_binding.status ==
                    viper::PlayerPcarTaskBindingStatus::complete &&
                    pcar_binding.binding.has_value() &&
                    pcar_binding.binding->stream_record_index == 0U &&
                    pcar_binding.binding->serialized_stream_cursor == 0x100U &&
                    pcar_binding.binding->record_bytes_offset == 4U &&
                    pcar_binding.binding->record_size == 0x1e4U,
                "player PCAR binding must retain its exact decoded stream identity");
        require(viper::bind_player_pcar_task_record(
                    decoded, serialized.bytes, car_record, 0x101U).status ==
                    viper::PlayerPcarTaskBindingStatus::cursor_mismatch,
                "player PCAR binding must reject a different logical cursor");
        auto altered_player_record = car_record;
        altered_player_record.back() ^= std::byte{1U};
        require(viper::bind_player_pcar_task_record(
                    decoded, serialized.bytes, altered_player_record, 0x100U).status ==
                    viper::PlayerPcarTaskBindingStatus::record_bytes_mismatch,
                "player PCAR binding must reject setup/stream byte disagreement");
        const std::array<viper::PhysicsTaskRecordInput, 2> duplicate_pcar_inputs{{
            {car_record}, {car_record}}};
        const auto duplicate_pcar_stream = viper::serialize_physics_task_records(
            duplicate_pcar_inputs, 0x100U);
        const auto duplicate_pcar_read = viper::read_physics_task_record_stream(
            duplicate_pcar_stream.bytes);
        require(viper::bind_player_pcar_task_record(
                    duplicate_pcar_read, duplicate_pcar_stream.bytes,
                    car_record, 0x100U).status ==
                    viper::PlayerPcarTaskBindingStatus::duplicate_player_records,
                "player PCAR binding must reject ambiguous duplicate PCAR entries");
        require(viper::read_physics_task_record_stream({}).status ==
                    viper::PhysicsTaskRecordStreamReadStatus::end_of_stream,
                "empty byte span must be an empty stream");

        const std::array<std::byte, 2> partial_cursor{std::byte{0U}, std::byte{0U}};
        const auto incomplete_cursor =
            viper::read_physics_task_record_stream(partial_cursor);
        require(incomplete_cursor.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::incomplete_cursor &&
                    incomplete_cursor.failed_record_index == 0U,
                "partial cursor prefix must be reported as incomplete");
        const auto incomplete_header = viper::read_physics_task_record_stream(
            std::span<const std::byte>(serialized.bytes.data(), 8U));
        require(incomplete_header.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::incomplete_record_header &&
                    incomplete_header.final_cursor == 0x100U,
                "partial record header must retain its cursor for diagnosis");
        const auto incomplete_payload = viper::read_physics_task_record_stream(
            std::span<const std::byte>(serialized.bytes.data(), 4U + 0x1E4U - 1U));
        require(incomplete_payload.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::incomplete_record_payload,
                "short record payload must be rejected");

        auto cursor_mismatch_bytes = serialized.bytes;
        cursor_mismatch_bytes[static_entry + 4U + 0x48U] = std::byte{0x8DU};
        const auto cursor_mismatch =
            viper::read_physics_task_record_stream(cursor_mismatch_bytes);
        require(cursor_mismatch.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::cursor_mismatch &&
                    cursor_mismatch.failed_record_index == 2U,
                "subsequent record cursor must match the previous class advance");

        auto unknown_tag_bytes = serialized.bytes;
        unknown_tag_bytes[4U] = std::byte{0x78U};
        unknown_tag_bytes[5U] = std::byte{0x56U};
        unknown_tag_bytes[6U] = std::byte{0x34U};
        unknown_tag_bytes[7U] = std::byte{0x12U};
        const auto unknown_tag_read =
            viper::read_physics_task_record_stream(unknown_tag_bytes);
        require(unknown_tag_read.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::unknown_record_tag &&
                    unknown_tag_read.failed_record_index == 0U,
                "reader must reject classes without recovered cursor advances");

        auto eof_stream = serialized.bytes;
        eof_stream.insert(eof_stream.end(), {
            std::byte{0xFFU}, std::byte{0xFFU}, std::byte{0xFFU}, std::byte{0xFFU}});
        const auto eof_read = viper::read_physics_task_record_stream(eof_stream);
        require(eof_read.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::end_of_stream &&
                    eof_read.records.size() == 3U &&
                    eof_read.consumed_bytes == eof_stream.size() &&
                    eof_read.final_cursor == 0x2C0U,
                "native negative cursor sentinel must end the decoded stream");
        eof_stream.push_back(std::byte{0U});
        require(viper::read_physics_task_record_stream(eof_stream).status ==
                    viper::PhysicsTaskRecordStreamReadStatus::
                        trailing_bytes_after_end_of_stream,
                "bytes after native end sentinel must be rejected");

        const std::array<std::byte, 7> short_record{};
        const std::array<viper::PhysicsTaskRecordInput, 1> short_input{{
            {short_record}}};
        const auto malformed = viper::serialize_physics_task_records(short_input);
        require(malformed.status == viper::PhysicsTaskRecordSerializationStatus::
                    malformed_record_header && malformed.bytes.empty(),
                "short serialized record must be rejected without output bytes");

        auto mismatched_record = make_physics_record(0x43484B50U, 8U);
        mismatched_record[4] = std::byte{9U};
        const std::array<viper::PhysicsTaskRecordInput, 1> mismatch_input{{
            {mismatched_record}}};
        const auto mismatch = viper::serialize_physics_task_records(mismatch_input);
        require(mismatch.status == viper::PhysicsTaskRecordSerializationStatus::
                    declared_size_mismatch && mismatch.bytes.empty(),
                "record size mismatch must be rejected before serialization");

        auto unknown_record = make_physics_record(0x12345678U, 8U);
        const std::array<viper::PhysicsTaskRecordInput, 1> unknown_input{{
            {unknown_record}}};
        const auto unknown_serialized =
            viper::serialize_physics_task_records(unknown_input);
        require(unknown_serialized.status ==
                    viper::PhysicsTaskRecordSerializationStatus::unknown_record_tag &&
                    unknown_serialized.bytes.empty(),
                "unknown class must not receive a fabricated cursor advance");

        const auto serialization_overflow = viper::serialize_physics_task_records(
            std::span<const viper::PhysicsTaskRecordInput>(input_records.data(), 1U),
            std::numeric_limits<std::uint32_t>::max() - 0x100U);
        require(serialization_overflow.status ==
                    viper::PhysicsTaskRecordSerializationStatus::cursor_overflow &&
                    serialization_overflow.bytes.empty(),
                "cursor overflow must reject a record before appending its bytes");
        const auto signed_sentinel_cursor = viper::serialize_physics_task_records(
            std::span<const viper::PhysicsTaskRecordInput>(input_records.data(), 1U),
            0x80000000U);
        require(signed_sentinel_cursor.status ==
                    viper::PhysicsTaskRecordSerializationStatus::cursor_overflow &&
                    signed_sentinel_cursor.bytes.empty(),
                "writer must reject cursors interpreted as the native EOF sentinel");

        const auto bemidji_prefix = make_track_object_table({
            "out\\track.txt", "map test.map", "ai test.vf",
            "obj car 1, 1", "obj car 2, 2", "obj car 3, 3", "obj car 4, 4",
            "obj car 5, 5", "obj car 6, 6", "obj car 7, 7", "obj car 8, 8",
            "obj checkpoint finish 1,1 2,2"});
        const auto player_cursor =
            viper::plan_track_player_physics_cursor(bemidji_prefix, 7U);
        require(player_cursor.status == viper::TrackPhysicsCursorStatus::complete &&
                    player_cursor.serialized_object_stream_offset == 0xAD4U &&
                    player_cursor.preceding_car_count == 7U &&
                    player_cursor.preceding_records.size() == 7U &&
                    player_cursor.preceding_records.front().object_row == 3U &&
                    player_cursor.preceding_records.front().car_directive_index == 0U &&
                    player_cursor.preceding_records.front().record_tag == 0x41434152U &&
                    player_cursor.preceding_records.front().cursor_offset == 0U &&
                    player_cursor.preceding_records.back().object_row == 9U &&
                    player_cursor.preceding_records.back().car_directive_index == 6U &&
                    player_cursor.preceding_records.back().cursor_offset == 0x948U,
                "Bemidji player cursor must fold seven preceding ACAR records to 0xAD4");

        const auto interleaved_object = make_track_object_table({
            "obj car 1, 1", "obj unknown_physics_object 2, 2", "obj car 3, 3"});
        const auto unresolved_cursor =
            viper::plan_track_player_physics_cursor(interleaved_object, 1U);
        require(unresolved_cursor.status ==
                    viper::TrackPhysicsCursorStatus::unsupported_prior_object &&
                    unresolved_cursor.unsupported_object_row == 1U &&
                    !unresolved_cursor.serialized_object_stream_offset.has_value(),
                "unmapped prior object must leave the player stream cursor unresolved");

        const auto mapped_interleaving = make_track_object_table({
            "obj wobble pole 0", "obj car 1, 1", "obj static flag 0 0 0 0 0 0 0 0 0",
            "obj car 2, 2"});
        const auto mapped_cursor =
            viper::plan_track_player_physics_cursor(mapped_interleaving, 1U);
        require(mapped_cursor.status == viper::TrackPhysicsCursorStatus::complete &&
                    mapped_cursor.serialized_object_stream_offset == 0x1C0U &&
                    mapped_cursor.preceding_car_count == 1U &&
                    mapped_cursor.preceding_records.size() == 3U &&
                    mapped_cursor.preceding_records[0].object_row == 0U &&
                    !mapped_cursor.preceding_records[0].car_directive_index.has_value() &&
                    mapped_cursor.preceding_records[0].record_tag == 0x574F424CU &&
                    mapped_cursor.preceding_records[0].cursor_offset == 0U &&
                    mapped_cursor.preceding_records[0].cursor_advance == 0x34U &&
                    mapped_cursor.preceding_records[1].object_row == 1U &&
                    mapped_cursor.preceding_records[1].car_directive_index == 0U &&
                    mapped_cursor.preceding_records[1].record_tag == 0x41434152U &&
                    mapped_cursor.preceding_records[1].cursor_offset == 0x34U &&
                    mapped_cursor.preceding_records[2].object_row == 2U &&
                    !mapped_cursor.preceding_records[2].car_directive_index.has_value() &&
                    mapped_cursor.preceding_records[2].record_tag == 0x53544154U &&
                    mapped_cursor.preceding_records[2].cursor_offset == 0x1C0U &&
                    mapped_cursor.preceding_records[2].cursor_advance == 0U,
                "wobble and static rows must contribute their recovered cursor advances");

        const auto known_scene_objects = make_track_object_table({
            "obj obstacle wall", "obj wobble pole 0", "obj checkpoint finish",
            "obj static flag", "obj car 1, 1"});
        const auto known_scene_cursor =
            viper::plan_track_player_physics_cursor(known_scene_objects, 0U);
        require(known_scene_cursor.status ==
                    viper::TrackPhysicsCursorStatus::complete &&
                    known_scene_cursor.serialized_object_stream_offset == 0x9cU &&
                    known_scene_cursor.preceding_records.size() == 4U &&
                    known_scene_cursor.preceding_records[0].record_tag == 0x4F425354U &&
                    known_scene_cursor.preceding_records[1].record_tag == 0x574F424CU &&
                    known_scene_cursor.preceding_records[2].record_tag == 0x43484B50U &&
                    known_scene_cursor.preceding_records[3].record_tag == 0x53544154U &&
                    known_scene_cursor.preceding_records[3].cursor_offset == 0x9cU &&
                    known_scene_cursor.preceding_records[3].cursor_advance == 0U,
                "known obstacle, wobble, checkpoint, and static prefixes must fold safely");

        const auto unknown_ball_directive = make_track_object_table({
            "obj ball ball", "obj car 1, 1"});
        const auto unknown_ball_cursor =
            viper::plan_track_player_physics_cursor(unknown_ball_directive, 0U);
        require(unknown_ball_cursor.status ==
                    viper::TrackPhysicsCursorStatus::unsupported_prior_object &&
                    unknown_ball_cursor.unsupported_object_row == 0U &&
                    !unknown_ball_cursor.serialized_object_stream_offset.has_value(),
                "configured BALL effect must not be accepted as an obj directive");

        const auto short_car_table = make_track_object_table({
            "obj car 1, 1", "obj car 2, 2"});
        const auto missing_cursor =
            viper::plan_track_player_physics_cursor(short_car_table, 2U);
        require(missing_cursor.status ==
                    viper::TrackPhysicsCursorStatus::player_car_not_found &&
                    !missing_cursor.serialized_object_stream_offset.has_value(),
                "missing player directive must not fabricate a stream cursor");

        constexpr std::array<std::uint32_t, 3> unknown_tail{
            0x50434152U, 0x12345678U, 0x4F425354U};
        const auto partial = viper::plan_physics_task_stream_offsets(unknown_tail);
        require(partial.status ==
                    viper::PhysicsTaskStreamPlanStatus::unknown_record_tag &&
                    partial.failed_record_index == 1U && partial.records.size() == 1U &&
                    partial.final_cursor == 0x18CU,
                "unknown tag did not stop the offset walk at its record");

        viper::PhysicsTaskRecordLoadObservation loaded_record{};
        loaded_record.serialized_object_stream_offset = 0xAD4;
        loaded_record.payload_complete = true;
        loaded_record.fourcc = 0x50434152U;
        loaded_record.allocation_succeeded = true;
        loaded_record.constructor_returned_object = true;
        loaded_record.panic_reporter_returned = true;
        const auto load_plan = viper::plan_physics_task_record_load(loaded_record);
        require(load_plan.status == viper::PhysicsTaskRecordLoadStatus::object_loaded &&
                    load_plan.constructor_argument_2_stream_offset == 0xAD4U,
                "record loader must propagate the signed cursor as constructor argument 2");

        loaded_record.serialized_object_stream_offset = -1;
        const auto end_plan = viper::plan_physics_task_record_load(loaded_record);
        require(end_plan.status == viper::PhysicsTaskRecordLoadStatus::end_of_records &&
                    !end_plan.constructor_argument_2_stream_offset.has_value() &&
                    !end_plan.payload_read,
                "negative serialized cursor is the native end-of-records sentinel");

        loaded_record.serialized_object_stream_offset = 0xAD4;
        loaded_record.payload_complete = false;
        const auto truncated_load =
            viper::plan_physics_task_record_load(loaded_record);
        require(truncated_load.status ==
                    viper::PhysicsTaskRecordLoadStatus::incomplete_payload &&
                    truncated_load.constructor_argument_2_stream_offset == 0xAD4U &&
                    truncated_load.factory_input_indeterminate,
                "failed record reads retain the already-read constructor cursor");

        auto stat_record = make_physics_record(0x53544154U, 0x48U);
        const auto write_stat_word = [&stat_record](const std::size_t offset,
                                                     const std::uint32_t value) {
            for (std::size_t byte = 0U; byte < 4U; ++byte) {
                stat_record[offset + byte] = static_cast<std::byte>(
                    (value >> (byte * 8U)) & 0xffU);
            }
        };
        for (std::size_t index = 0U; index < 12U; ++index)
            write_stat_word(0x08U + index * 4U,
                0x100U + static_cast<std::uint32_t>(index));
        write_stat_word(0x38U, 0U);
        write_stat_word(0x3cU, std::bit_cast<std::uint32_t>(1.0F));
        write_stat_word(0x40U, std::bit_cast<std::uint32_t>(2.0F));
        write_stat_word(0x44U, std::bit_cast<std::uint32_t>(3.0F));
        const auto stat_state = viper::materialize_inactive_stat_task_runtime_state(
            stat_record, 0x80U);
        require(stat_state.status ==
                    viper::PhysicsStatTaskRuntimeMaterializationStatus::complete &&
                    stat_state.state.has_value() &&
                    stat_state.state->serialized_stream_cursor == 0x80U &&
                    stat_state.state->copied_record_words_0x38_to_0x64[0] == 0x100U &&
                    stat_state.state->copied_record_words_0x38_to_0x64[11] == 0x10bU &&
                    stat_state.state->child_center == std::array<float, 3>{1.0F, 2.0F, 3.0F} &&
                    stat_state.state->child_bound_0x28 == 3.0F &&
                    stat_state.state->child_count == 1U &&
                    !stat_state.state->active_gate &&
                    stat_state.state->update_callback_is_noop &&
                    stat_state.state->virtual_0x18_size == 0x12U,
                "STAT inactive runtime materialization lost constructor or vtable state");
        write_stat_word(0x38U, 1U);
        const auto unsupported_stat =
            viper::materialize_inactive_stat_task_runtime_state(stat_record, 0x80U);
        require(unsupported_stat.status ==
                    viper::PhysicsStatTaskRuntimeMaterializationStatus::
                        unsupported_collision_volume && !unsupported_stat.state.has_value(),
                "STAT collision-volume constructor was treated as supported");
        write_stat_word(4U, 0x47U);
        const auto malformed_stat =
            viper::materialize_inactive_stat_task_runtime_state(stat_record, 0x80U);
        require(malformed_stat.status ==
                    viper::PhysicsStatTaskRuntimeMaterializationStatus::malformed_record &&
                    !malformed_stat.state.has_value(),
                "STAT runtime materializer accepted an inconsistent declared size");
        write_stat_word(0x38U, 0U);
        write_stat_word(4U, 0x48U);

        auto checkpoint_record = make_physics_record(0x43484b50U, 0x20U);
        const auto write_checkpoint_word = [&checkpoint_record](
            const std::size_t offset, const std::uint32_t value) {
            for (std::size_t byte = 0U; byte < 4U; ++byte) {
                checkpoint_record[offset + byte] = static_cast<std::byte>(
                    (value >> (byte * 8U)) & 0xffU);
            }
        };
        const std::array<float, 6> checkpoint_endpoints{
            0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 10.0F};
        for (std::size_t field = 0U; field < checkpoint_endpoints.size(); ++field)
            write_checkpoint_word(0x08U + field * 4U,
                std::bit_cast<std::uint32_t>(checkpoint_endpoints[field]));
        auto unsupported_active_record = make_physics_record(0x4f425354U, 0x38U);
        const std::array<viper::PhysicsTaskRecordInput, 3> runtime_records{{
            {checkpoint_record}, {stat_record}, {unsupported_active_record}}};
        const auto runtime_serialized = viper::serialize_physics_task_records(
            runtime_records, 0x40U);
        const auto runtime_decoded = viper::read_physics_task_record_stream(
            runtime_serialized.bytes);
        const auto runtime_tasks = viper::materialize_physics_task_runtime_collection(
            runtime_decoded, runtime_serialized.bytes, false);
        require(runtime_serialized.status ==
                    viper::PhysicsTaskRecordSerializationStatus::complete &&
                    runtime_decoded.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::complete &&
                    runtime_tasks.status ==
                    viper::PhysicsTaskRuntimeCollectionStatus::complete &&
                    runtime_tasks.entries.size() == 3U,
                "semantic task runtime did not retain the complete ordered stream");
        require(runtime_tasks.entries[0].record_tag == 0x43484b50U &&
                    runtime_tasks.entries[0].stream_record_index == 0U &&
                    runtime_tasks.entries[0].stream_cursor == 0x40U &&
                    runtime_tasks.entries[0].status ==
                        viper::PhysicsTaskRuntimeEntryStatus::inert_checkpoint_retained &&
                    runtime_tasks.entries[0].checkpoint.has_value() &&
                    runtime_tasks.entries[0].checkpoint->first_vertex ==
                        std::array<float, 3>{0.0F, 0.0F, 0.0F} &&
                    runtime_tasks.entries[0].checkpoint->second_vertex ==
                        std::array<float, 3>{0.0F, 0.0F, 10.0F} &&
                    runtime_tasks.entries[0].checkpoint->constructor.center ==
                        std::array<float, 3>{0.0F, 0.0F, 5.0F} &&
                    !runtime_tasks.entries[0].checkpoint->active_gate &&
                    runtime_tasks.entries[0].checkpoint->update_callback_is_noop,
                "CHKP record did not become retained inert checkpoint geometry");
        require(runtime_tasks.entries[1].record_tag == 0x53544154U &&
                    runtime_tasks.entries[1].status ==
                        viper::PhysicsTaskRuntimeEntryStatus::inactive_stat_retained &&
                    runtime_tasks.entries[1].stat.has_value() &&
                    runtime_tasks.entries[1].stat->serialized_stream_cursor ==
                        runtime_decoded.records[1].cursor_offset &&
                    runtime_tasks.entries[2].record_tag == 0x4f425354U &&
                    runtime_tasks.entries[2].status ==
                        viper::PhysicsTaskRuntimeEntryStatus::
                            obstacle_cached_pose_retained &&
                    runtime_tasks.entries[2].callback_behavior_unavailable &&
                    runtime_tasks.entries[2].obstacle.has_value() &&
                    runtime_tasks.entries[2].obstacle->geometry_records.empty() &&
                    !runtime_tasks.entries[2].checkpoint.has_value() &&
                    !runtime_tasks.entries[2].stat.has_value(),
                "semantic task runtime did not preserve OBST pose and unavailable callback status");

        // Synthetic fixture for the captured factory order: seven ACAR cars,
        // the player PCAR, then three CHKP objects. This verifies serialized
        // semantics only; it does not model native allocation or dispatch.
        std::array<std::vector<std::byte>, 7> acar_records;
        for (auto& record : acar_records)
            record = make_physics_record(0x41434152U, 0x1E4U);
        auto pcar_record = make_physics_record(0x50434152U, 0x1E4U);
        std::array<std::vector<std::byte>, 3> checkpoint_records;
        constexpr std::array<std::array<float, 6>, 3> synthetic_checkpoint_endpoints{{
            {{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 10.0F}},
            {{2.0F, 1.0F, -3.0F, 8.0F, 1.0F, -3.0F}},
            {{-4.0F, 2.0F, 5.0F, -4.0F, 12.0F, 9.0F}},
        }};
        for (std::size_t checkpoint_index = 0U;
             checkpoint_index < checkpoint_records.size(); ++checkpoint_index) {
            auto& record = checkpoint_records[checkpoint_index];
            record = make_physics_record(0x43484B50U, 0x20U);
            for (std::size_t field = 0U; field < 6U; ++field) {
                const auto value = std::bit_cast<std::uint32_t>(
                    synthetic_checkpoint_endpoints[checkpoint_index][field]);
                for (std::size_t byte = 0U; byte < 4U; ++byte)
                    record[0x08U + field * 4U + byte] =
                        static_cast<std::byte>((value >> (byte * 8U)) & 0xFFU);
            }
        }
        std::array<viper::PhysicsTaskRecordInput, 11> captured_factory_inputs{};
        for (std::size_t index = 0U; index < acar_records.size(); ++index)
            captured_factory_inputs[index].record_bytes = acar_records[index];
        captured_factory_inputs[7U].record_bytes = pcar_record;
        for (std::size_t index = 0U; index < checkpoint_records.size(); ++index)
            captured_factory_inputs[8U + index].record_bytes = checkpoint_records[index];

        constexpr std::array<std::uint32_t, 11> captured_factory_cursors{
            0x000U, 0x18CU, 0x318U, 0x4A4U, 0x630U, 0x7BCU,
            0x948U, 0xAD4U, 0xC60U, 0xC94U, 0xCC8U};
        const auto captured_factory_stream = viper::serialize_physics_task_records(
            captured_factory_inputs);
        const auto captured_factory_decoded = viper::read_physics_task_record_stream(
            captured_factory_stream.bytes);
        const auto captured_factory_inventory = viper::inventory_physics_task_objects(
            captured_factory_decoded, captured_factory_stream.bytes);
        const auto captured_factory_runtime =
            viper::materialize_physics_task_runtime_collection(
                captured_factory_decoded, captured_factory_stream.bytes, false);
        constexpr std::array<std::uint32_t, 11> captured_factory_tags{
            0x41434152U, 0x41434152U, 0x41434152U, 0x41434152U,
            0x41434152U, 0x41434152U, 0x41434152U, 0x50434152U,
            0x43484B50U, 0x43484B50U, 0x43484B50U};
        require(captured_factory_stream.status ==
                    viper::PhysicsTaskRecordSerializationStatus::complete &&
                    captured_factory_decoded.status ==
                        viper::PhysicsTaskRecordStreamReadStatus::complete &&
                    captured_factory_stream.records.size() == captured_factory_tags.size() &&
                    captured_factory_decoded.records.size() == captured_factory_tags.size() &&
                    captured_factory_stream.final_cursor == 0xCFCU &&
                    captured_factory_decoded.final_cursor == 0xCFCU,
                "captured factory fixture did not serialize and decode all eleven records");
        require(captured_factory_inventory.status ==
                    viper::PhysicsTaskObjectInventoryStatus::complete &&
                    captured_factory_inventory.entries.size() == captured_factory_tags.size() &&
                    captured_factory_runtime.status ==
                    viper::PhysicsTaskRuntimeCollectionStatus::complete &&
                    captured_factory_runtime.entries.size() == captured_factory_tags.size(),
                "captured factory fixture did not retain complete inventory and runtime collections");
        for (std::size_t index = 0U; index < captured_factory_tags.size(); ++index) {
            const auto& stream_record = captured_factory_decoded.records[index];
            const auto& inventory_entry = captured_factory_inventory.entries[index];
            const auto& runtime_entry = captured_factory_runtime.entries[index];
            require(stream_record.record_tag == captured_factory_tags[index] &&
                        stream_record.cursor_offset == captured_factory_cursors[index] &&
                        inventory_entry.stream_record_index == index &&
                        inventory_entry.record_tag == captured_factory_tags[index] &&
                        inventory_entry.stream_cursor == captured_factory_cursors[index] &&
                        runtime_entry.stream_record_index == index &&
                        runtime_entry.record_tag == captured_factory_tags[index] &&
                        runtime_entry.stream_cursor == captured_factory_cursors[index],
                    "captured factory order or cursor diverged across stream, inventory, and runtime");
            require(!inventory_entry.native_object_materialized &&
                        !inventory_entry.fixed_step_callback_supported,
                    "synthetic task inventory overclaimed native construction or callback support");
            if (index < 8U) {
                require(stream_record.record_size == 0x1E4U &&
                            inventory_entry.record_size == 0x1E4U &&
                            runtime_entry.status ==
                                viper::PhysicsTaskRuntimeEntryStatus::
                                    active_callback_unavailable &&
                            !runtime_entry.checkpoint.has_value(),
                        "ACAR or PCAR fixture must remain explicitly unavailable for active dispatch");
            } else {
                const auto checkpoint_index = index - 8U;
                require(stream_record.record_size == 0x20U &&
                            inventory_entry.record_size == 0x20U &&
                            captured_factory_stream.records[index].cursor_advance == 0x34U &&
                            runtime_entry.status ==
                                viper::PhysicsTaskRuntimeEntryStatus::
                                    inert_checkpoint_retained &&
                            runtime_entry.checkpoint.has_value() &&
                            !runtime_entry.checkpoint->active_gate &&
                            runtime_entry.checkpoint->update_callback_is_noop,
                        "CHKP fixture must retain inert geometry with the observed 0x34 cursor advance");
                for (std::size_t field = 0U; field < 6U; ++field) {
                    const auto& endpoint = field < 3U
                        ? runtime_entry.checkpoint->first_vertex
                        : runtime_entry.checkpoint->second_vertex;
                    require(endpoint[field % 3U] ==
                                synthetic_checkpoint_endpoints[checkpoint_index][field],
                            "CHKP fixture endpoint geometry changed during runtime materialization");
                }
            }
        }

        auto selector_two_obstacle = make_physics_record(0x4f425354U, 0x38U);
        const auto write_obstacle_word = [&selector_two_obstacle](
            const std::size_t offset, const std::uint32_t value) {
            for (std::size_t byte = 0U; byte < 4U; ++byte) {
                selector_two_obstacle[offset + byte] = static_cast<std::byte>(
                    (value >> (byte * 8U)) & 0xffU);
            }
        };
        const auto write_obstacle_float = [&write_obstacle_word](
            const std::size_t offset, const float value) {
            write_obstacle_word(offset, std::bit_cast<std::uint32_t>(value));
        };
        write_obstacle_float(0x08U, 1.5F);
        write_obstacle_float(0x18U, 12.0F);
        write_obstacle_float(0x1cU, 3.0F);
        write_obstacle_float(0x20U, -7.0F);
        write_obstacle_float(0x24U, 90.0F);
        write_obstacle_float(0x28U, 2.0F);
        write_obstacle_float(0x2cU, 4.0F);
        write_obstacle_float(0x30U, 6.0F);
        write_obstacle_word(0x34U, 2U);
        const std::array<viper::PhysicsTaskRecordInput, 1> obstacle_input{{
            {selector_two_obstacle}}};
        const auto obstacle_stream = viper::serialize_physics_task_records(
            obstacle_input);
        const auto obstacle_decoded = viper::read_physics_task_record_stream(
            obstacle_stream.bytes);
        const auto obstacle_runtime =
            viper::materialize_physics_task_runtime_collection(
                obstacle_decoded, obstacle_stream.bytes, false);
        require(obstacle_stream.status ==
                    viper::PhysicsTaskRecordSerializationStatus::complete &&
                    obstacle_decoded.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::complete &&
                    obstacle_runtime.status ==
                    viper::PhysicsTaskRuntimeCollectionStatus::complete &&
                    obstacle_runtime.entries.size() == 1U,
                "selector-2 OBST record did not reach ordered runtime materialization");
        const auto& selector_two_runtime_entry = obstacle_runtime.entries.front();
        require(selector_two_runtime_entry.status == viper::PhysicsTaskRuntimeEntryStatus::
                    obstacle_shape_geometry_retained &&
                    selector_two_runtime_entry.callback_behavior_unavailable &&
                    selector_two_runtime_entry.obstacle.has_value() &&
                    selector_two_runtime_entry.obstacle->shape_selector_0x34 == 2U &&
                    selector_two_runtime_entry.obstacle->geometry_records.size() == 6U &&
                    selector_two_runtime_entry.obstacle->cached_position_words ==
                        std::array<std::uint32_t, 3>{
                            std::bit_cast<std::uint32_t>(12.0F),
                            std::bit_cast<std::uint32_t>(3.0F),
                            std::bit_cast<std::uint32_t>(-7.0F)} &&
                    std::abs(selector_two_runtime_entry.obstacle->sphere_scale - 24.525F) <
                        1.0e-4F &&
                    !selector_two_runtime_entry.obstacle->timer_value_known &&
                    !selector_two_runtime_entry.obstacle->native_object_allocation_known,
                "selector-2 OBST state must retain sourced geometry and expose unknown timer/allocation");
        const auto& first_obstacle_geometry =
            selector_two_runtime_entry.obstacle->geometry_records.front();
        require(first_obstacle_geometry.object_offset == 0x6cU &&
                    first_obstacle_geometry.known_word_mask == 0x07ffU &&
                    std::bit_cast<float>(first_obstacle_geometry.words[0]) == 1.0F &&
                    std::bit_cast<float>(first_obstacle_geometry.words[1]) == -2.0F &&
                    std::bit_cast<float>(first_obstacle_geometry.words[2]) == -3.0F &&
                    first_obstacle_geometry.words[6] ==
                        std::bit_cast<std::uint32_t>(24.525F),
                "OBST box geometry projection must preserve native offsets, known mask, and dimensions");

        auto other_selector_obstacle = selector_two_obstacle;
        for (std::size_t byte = 0U; byte < 4U; ++byte) {
            other_selector_obstacle[0x34U + byte] = std::byte{0};
        }
        const std::array<viper::PhysicsTaskRecordInput, 1> other_selector_input{{
            {other_selector_obstacle}}};
        const auto other_selector_stream = viper::serialize_physics_task_records(
            other_selector_input);
        const auto other_selector_decoded = viper::read_physics_task_record_stream(
            other_selector_stream.bytes);
        const auto other_selector_runtime =
            viper::materialize_physics_task_runtime_collection(
                other_selector_decoded, other_selector_stream.bytes, false);
        require(other_selector_runtime.status ==
                    viper::PhysicsTaskRuntimeCollectionStatus::complete &&
                    other_selector_runtime.entries.front().status ==
                        viper::PhysicsTaskRuntimeEntryStatus::
                            obstacle_cached_pose_retained &&
                    other_selector_runtime.entries.front().obstacle.has_value() &&
                    other_selector_runtime.entries.front().obstacle->geometry_records.empty() &&
                    other_selector_runtime.entries.front().obstacle->cached_position_words ==
                        selector_two_runtime_entry.obstacle->cached_position_words,
                "non-box OBST selectors must retain only their shared cached pose");

        auto invalid_obstacle = selector_two_obstacle;
        const auto invalid_yaw = std::bit_cast<std::uint32_t>(
            std::numeric_limits<float>::infinity());
        for (std::size_t byte = 0U; byte < 4U; ++byte) {
            invalid_obstacle[0x24U + byte] = static_cast<std::byte>(
                (invalid_yaw >> (byte * 8U)) & 0xffU);
        }
        const std::array<viper::PhysicsTaskRecordInput, 1> invalid_obstacle_input{{
            {invalid_obstacle}}};
        const auto invalid_obstacle_stream = viper::serialize_physics_task_records(
            invalid_obstacle_input);
        const auto invalid_obstacle_decoded = viper::read_physics_task_record_stream(
            invalid_obstacle_stream.bytes);
        const auto invalid_obstacle_runtime =
            viper::materialize_physics_task_runtime_collection(
                invalid_obstacle_decoded, invalid_obstacle_stream.bytes, false);
        require(invalid_obstacle_runtime.status ==
                    viper::PhysicsTaskRuntimeCollectionStatus::
                        entry_materialization_failed &&
                    invalid_obstacle_runtime.failed_record_index == 0U &&
                    invalid_obstacle_runtime.entries.empty(),
                "selector-2 OBST runtime must reject non-finite constructor inputs");

        const auto truncated_obstacle = make_physics_record(0x4f425354U, 0x34U);
        const std::array<viper::PhysicsTaskRecordInput, 1> truncated_obstacle_input{{
            {truncated_obstacle}}};
        const auto truncated_obstacle_stream = viper::serialize_physics_task_records(
            truncated_obstacle_input);
        const auto truncated_obstacle_decoded = viper::read_physics_task_record_stream(
            truncated_obstacle_stream.bytes);
        const auto truncated_obstacle_runtime =
            viper::materialize_physics_task_runtime_collection(
                truncated_obstacle_decoded, truncated_obstacle_stream.bytes, false);
        require(truncated_obstacle_runtime.status ==
                    viper::PhysicsTaskRuntimeCollectionStatus::
                        entry_materialization_failed &&
                    truncated_obstacle_runtime.failed_record_index == 0U,
                "OBST runtime must reject a record too short for its selector and constructor inputs");

        auto invalid_runtime_bytes = runtime_serialized.bytes;
        const auto write_nan_word = [&invalid_runtime_bytes](
            const std::size_t offset, const std::uint32_t value) {
            for (std::size_t byte = 0U; byte < 4U; ++byte) {
                invalid_runtime_bytes[offset + byte] = static_cast<std::byte>(
                    (value >> (byte * 8U)) & 0xffU);
            }
        };
        write_nan_word(4U + 0x08U, 0x7fc00000U);
        const auto invalid_runtime_read =
            viper::read_physics_task_record_stream(invalid_runtime_bytes);
        const auto invalid_runtime = viper::materialize_physics_task_runtime_collection(
            invalid_runtime_read, invalid_runtime_bytes, false);
        require(invalid_runtime.status ==
                    viper::PhysicsTaskRuntimeCollectionStatus::
                        entry_materialization_failed &&
                    invalid_runtime.failed_record_index == 0U &&
                    invalid_runtime.entries.empty(),
                "semantic task runtime accepted non-finite CHKP endpoints");

        constexpr std::array<std::uint32_t, 1> car_tag{0x50434152U};
        const auto overflow = viper::plan_physics_task_stream_offsets(
            car_tag, std::numeric_limits<std::uint32_t>::max() - 0x100U);
        require(overflow.status ==
                    viper::PhysicsTaskStreamPlanStatus::cursor_overflow &&
                    overflow.failed_record_index == 0U && overflow.records.empty(),
                "cursor overflow fabricated a record offset");
        std::cout << "physics task stream regression passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
