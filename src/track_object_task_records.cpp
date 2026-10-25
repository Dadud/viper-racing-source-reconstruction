#include "viper/track_object_task_records.hpp"

#include "viper/model_resource.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <string>

namespace viper {
namespace {

bool ascii_equal_case_insensitive(const std::string_view left,
                                  const std::string_view right) noexcept
{
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        auto a = static_cast<unsigned char>(left[i]);
        auto b = static_cast<unsigned char>(right[i]);
        if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return true;
}

void append_u32_le(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    for (unsigned int shift = 0U; shift < 32U; shift += 8U)
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void append_f32_le(std::vector<std::byte>& bytes, const float value)
{
    append_u32_le(bytes, std::bit_cast<std::uint32_t>(value));
}

std::vector<std::byte> serialize_wobble(const WobbleTrackRecord& record)
{
    std::vector<std::byte> bytes;
    bytes.reserve(sizeof(WobbleTrackRecord));
    append_u32_le(bytes, record.object_tag);
    append_u32_le(bytes, record.record_size);
    append_f32_le(bytes, record.raw_0x08);
    append_f32_le(bytes, record.raw_0x0c);
    append_f32_le(bytes, record.raw_0x10);
    append_f32_le(bytes, record.raw_0x14);
    append_u32_le(bytes, std::bit_cast<std::uint32_t>(record.parsed_parameter_0x18));
    append_u32_le(bytes, static_cast<std::uint32_t>(record.type_0x1c));
    return bytes;
}

std::vector<std::byte> serialize_static(const StaticTrackRecord& record)
{
    std::vector<std::byte> bytes;
    bytes.reserve(sizeof(StaticTrackRecord));
    append_u32_le(bytes, record.object_tag);
    append_u32_le(bytes, record.record_size);
    for (const auto value : record.payload.rotation_matrix_0x08)
        append_f32_le(bytes, value);
    for (const auto value : record.payload.parsed_vector_0x2c)
        append_f32_le(bytes, value);
    append_u32_le(bytes, record.payload.raw_word_0x38);
    for (const auto value : record.payload.parsed_vector_0x3c)
        append_f32_le(bytes, value);
    return bytes;
}

std::vector<std::byte> serialize_obstacle(const ObstacleTrackRecord& record)
{
    std::vector<std::byte> bytes;
    bytes.reserve(sizeof(ObstacleTrackRecord));
    append_u32_le(bytes, record.object_tag);
    append_u32_le(bytes, record.record_size);
    append_f32_le(bytes, record.scale_0x08);
    append_f32_le(bytes, record.scaled_extent_term_y_0x0c);
    append_f32_le(bytes, record.scaled_extent_term_z_0x10);
    append_f32_le(bytes, record.scaled_extent_term_x_0x14);
    append_f32_le(bytes, record.x_0x18);
    append_f32_le(bytes, record.adjusted_y_0x1c);
    append_f32_le(bytes, record.z_0x20);
    append_f32_le(bytes, record.parsed_parameter_0x24);
    append_f32_le(bytes, record.extent_x_0x28);
    append_f32_le(bytes, record.extent_y_0x2c);
    append_f32_le(bytes, record.extent_z_0x30);
    append_u32_le(bytes, static_cast<std::uint32_t>(record.type_0x34));
    return bytes;
}

std::optional<ModelResource> find_model(
    const std::span<const ResourceBundle* const> bundles,
    const std::string_view model_name,
    bool& invalid_resource)
{
    for (const auto* bundle : bundles) {
        if (bundle == nullptr) continue;
        const auto* entry = bundle->find(model_name, ModelResource::resource_type_code);
        if (entry != nullptr) {
            auto model = parse_model_resource(entry->attributes, bundle->payload(*entry));
            if (!model) {
                invalid_resource = true;
                return std::nullopt;
            }
            return model;
        }
    }
    return std::nullopt;
}

void parse_wobble_line(const std::string_view raw,
                       WobbleTrackLineObservation& observation,
                       std::array<char, 256>& type)
{
    std::string line(raw);
    int parameter{};
    const auto count = std::sscanf(line.c_str(), "obj wobble %255s %d",
        type.data(), &parameter);
    if (count >= 1) observation.type_name = type.data();
    if (count >= 2) observation.parsed_parameter = parameter;
    observation.scanf_conversion_count = count < 0 ? 0U :
        static_cast<std::uint32_t>(count);
}

void parse_static_line(const std::string_view raw,
                       StaticTrackLineObservation& observation,
                       std::array<char, 256>& type)
{
    std::string line(raw);
    std::array<float, 9> values{};
    const auto count = std::sscanf(line.c_str(),
        "obj static %255s %f %f %f %f %f %f %f %f %f",
        type.data(), &values[0], &values[1], &values[2], &values[3],
        &values[4], &values[5], &values[6], &values[7], &values[8]);
    if (count >= 1) observation.type_name = type.data();
    observation.scanf_conversion_count = count < 0 ? 0U :
        static_cast<std::uint32_t>(count);
    if (count > 1) observation.parsed_vector_0x2c = {values[0], values[1], values[2]};
    if (count > 4) observation.rotation_vector = {values[3], values[4], values[5]};
    if (count > 7) observation.parsed_vector_0x3c = {values[6], values[7], values[8]};
}

void parse_obstacle_line(const std::string_view raw,
                         ObstacleTrackLineObservation& observation,
                         std::array<char, 256>& type,
                         std::array<char, 256>& asset)
{
    std::string line(raw);
    const auto count = std::sscanf(line.c_str(),
        "obj obstacle %255s %255s %f,%f:%f %f", type.data(), asset.data(),
        &observation.x, &observation.z, &observation.raw_parameter_0x14,
        &observation.scale);
    if (count >= 1) observation.type_name = type.data();
    if (count >= 2) observation.asset_name = asset.data();
    observation.scanf_conversion_count = count < 0 ? 0U :
        static_cast<std::uint32_t>(count);
}

TrackObjectTaskRecordRow make_row(const std::size_t source_row,
                                  const TrackObjectTaskRecordStatus status,
                                  const std::string_view source_line)
{
    return {source_row, status, 0U, {}, std::string(source_line)};
}

} // namespace

TrackObjectTaskRecordMaterialization materialize_track_object_task_records(
    const TrackObjectTextTable& table,
    const TerrainSurfaceResourcePayload& terrain,
    const std::span<const ResourceBundle* const> model_bundles,
    const std::uint32_t track_state_selector_0x53b194,
    const bool static_object_dispatch_enabled,
    const bool compatibility_division_enabled)
{
    TrackObjectTaskRecordMaterialization result;
    result.complete = true;
    // Native 0x45C630 starts the object directive scan at line index one.
    for (std::size_t row_index = 1U; row_index < table.line_count; ++row_index) {
        const auto source = track_object_text_line(table, row_index);
        if (!source.has_value()) {
            result.rows.push_back(make_row(row_index,
                TrackObjectTaskRecordStatus::incomplete_track_data, {}));
            result.complete = false;
            break;
        }
        const auto directive = classify_track_directive(*source);
        if (directive.kind == TrackDirectiveKind::end) {
            result.rows.push_back(make_row(row_index,
                TrackObjectTaskRecordStatus::end_of_track_directives, *source));
            result.stopped_at_end = true;
            break;
        }
        if (directive.kind == TrackDirectiveKind::empty_or_unscannable ||
            directive.kind == TrackDirectiveKind::comment ||
            directive.kind == TrackDirectiveKind::map ||
            directive.kind == TrackDirectiveKind::ai) {
            result.rows.push_back(make_row(row_index,
                TrackObjectTaskRecordStatus::ignored_directive, *source));
            continue;
        }
        if (directive.kind == TrackDirectiveKind::bad_token) {
            result.rows.push_back(make_row(row_index,
                TrackObjectTaskRecordStatus::unsupported_object_type, *source));
            result.complete = false;
            break;
        }

        auto object_line = directive.object_line;
        while (!object_line.empty() &&
            (object_line.front() == ' ' || object_line.front() == '\t' ||
             object_line.front() == '\r' || object_line.front() == '\n'))
            object_line.remove_prefix(1U);
        if (object_line.size() < 3U ||
            !ascii_equal_case_insensitive(object_line.substr(0U, 3U), "obj")) {
            result.rows.push_back(make_row(row_index,
                TrackObjectTaskRecordStatus::unsupported_object_type, *source));
            result.complete = false;
            break;
        }
        object_line.remove_prefix(3U);
        while (!object_line.empty() &&
            (object_line.front() == ' ' || object_line.front() == '\t'))
            object_line.remove_prefix(1U);
        const auto type_end = object_line.find_first_of(" \t\r\n");
        const auto object_type = object_line.substr(0U,
            type_end == std::string_view::npos ? object_line.size() : type_end);

        if (ascii_equal_case_insensitive(object_type, "car")) {
            result.rows.push_back(make_row(row_index,
                TrackObjectTaskRecordStatus::car_record_deferred, *source));
            result.complete = false;
            continue;
        }
        if (ascii_equal_case_insensitive(object_type, "checkpoint")) {
            auto production = produce_checkpoint_task_record_from_line(*source,
                terrain, track_state_selector_0x53b194,
                compatibility_division_enabled);
            TrackObjectTaskRecordRow row = make_row(row_index,
                TrackObjectTaskRecordStatus::malformed_line, *source);
            if (production.status == CheckpointTaskRecordProductionStatus::visual_scene_object) {
                row.status = TrackObjectTaskRecordStatus::visual_only;
            }
            else if (production.serialized_record_bytes.has_value()) {
                row.status = TrackObjectTaskRecordStatus::complete_record;
                row.record_tag = 0x43484B50U;
                row.record_bytes.assign(production.serialized_record_bytes->begin(),
                    production.serialized_record_bytes->end());
            }
            else {
                result.complete = false;
            }
            result.rows.push_back(std::move(row));
            continue;
        }
        if (ascii_equal_case_insensitive(object_type, "wobble")) {
            WobbleTrackLineObservation observation{};
            std::array<char, 256> type{};
            parse_wobble_line(*source, observation, type);
            const auto record = make_wobble_track_record(observation);
            auto row = make_row(row_index, TrackObjectTaskRecordStatus::malformed_line, *source);
            if (record.has_value()) {
                row.status = TrackObjectTaskRecordStatus::complete_record;
                row.record_tag = record->object_tag;
                row.record_bytes = serialize_wobble(*record);
            } else {
                if (observation.scanf_conversion_count == 2U)
                    row.status = TrackObjectTaskRecordStatus::unsupported_object_type;
                result.complete = false;
            }
            result.rows.push_back(std::move(row));
            continue;
        }
        if (ascii_equal_case_insensitive(object_type, "static")) {
            if (!static_object_dispatch_enabled) {
                result.rows.push_back(make_row(row_index,
                    TrackObjectTaskRecordStatus::skipped_by_native_gate, *source));
                continue;
            }
            StaticTrackLineObservation observation{};
            observation.parser_enabled_by_dispatch = true;
            std::array<char, 256> type{};
            parse_static_line(*source, observation, type);
            const auto plan = plan_static_track_line(observation);
            auto row = make_row(row_index, TrackObjectTaskRecordStatus::malformed_line, *source);
            if (plan.record.has_value()) {
                row.status = TrackObjectTaskRecordStatus::complete_record;
                row.record_tag = plan.record->object_tag;
                row.record_bytes = serialize_static(*plan.record);
            } else {
                if (plan.disposition == StaticTrackLineDisposition::unknown_type)
                    row.status = TrackObjectTaskRecordStatus::unsupported_object_type;
                result.complete = false;
            }
            result.rows.push_back(std::move(row));
            continue;
        }
        if (ascii_equal_case_insensitive(object_type, "obstacle")) {
            ObstacleTrackLineObservation observation{};
            std::array<char, 256> type{};
            std::array<char, 256> asset{};
            parse_obstacle_line(*source, observation, type, asset);
            auto row = make_row(row_index, TrackObjectTaskRecordStatus::malformed_line, *source);
            if (observation.scanf_conversion_count != 6U) {
                result.complete = false;
                result.rows.push_back(std::move(row));
                continue;
            }
            if (!ascii_equal_case_insensitive(observation.type_name, "ball") &&
                !ascii_equal_case_insensitive(observation.type_name, "cube") &&
                !ascii_equal_case_insensitive(observation.type_name, "prism")) {
                row.status = TrackObjectTaskRecordStatus::unsupported_object_type;
                result.complete = false;
                result.rows.push_back(std::move(row));
                continue;
            }
            bool invalid_model = false;
            const auto model = find_model(model_bundles, observation.asset_name,
                invalid_model);
            if (!model.has_value()) {
                row.status = invalid_model
                    ? TrackObjectTaskRecordStatus::invalid_model_resource
                    : TrackObjectTaskRecordStatus::missing_model_resource;
                result.complete = false;
                result.rows.push_back(std::move(row));
                continue;
            }
            std::vector<ObstacleModelVertexRecord> vertices;
            vertices.reserve(model->vertices.size());
            for (const auto& vertex : model->vertices)
                vertices.push_back({vertex.fields});
            const auto bounds = compute_obstacle_model_bounds({
                true, static_cast<std::int32_t>(model->vertices.size()), vertices});
            if (!bounds.observation_complete || bounds.fatal_helper_called) {
                row.status = TrackObjectTaskRecordStatus::invalid_model_resource;
                result.complete = false;
                result.rows.push_back(std::move(row));
                continue;
            }
            observation.terrain_height = terrain_height_or_zero(terrain,
                observation.x, observation.z, compatibility_division_enabled);
            observation.model_bounds = bounds.bounds;
            const auto plan = plan_obstacle_track_line(observation);
            if (!plan.construction.has_value()) {
                row.status = TrackObjectTaskRecordStatus::unsupported_object_type;
                result.complete = false;
            } else {
                row.status = TrackObjectTaskRecordStatus::complete_record;
                row.record_tag = plan.construction->record.object_tag;
                row.record_bytes = serialize_obstacle(plan.construction->record);
            }
            result.rows.push_back(std::move(row));
            continue;
        }

        result.rows.push_back(make_row(row_index,
            TrackObjectTaskRecordStatus::unsupported_object_type, *source));
        result.complete = false;
        break;
    }
    return result;
}

OrderedTrackTaskRecordPlan plan_ordered_track_task_records(
    const TrackObjectTaskRecordMaterialization& materialization,
    const TrackPhysicsTaskManifest& manifest)
{
    OrderedTrackTaskRecordPlan result;
    result.complete = materialization.complete &&
        manifest.status == TrackPhysicsTaskManifestStatus::complete &&
        manifest.final_cursor.has_value();
    result.records.reserve(manifest.entries.size());

    const auto read_u32_le = [](const std::span<const std::byte> bytes,
                                const std::size_t offset) {
        return std::to_integer<std::uint32_t>(bytes[offset]) |
            (std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U) |
            (std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U) |
            (std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U);
    };

    for (std::size_t index = 0; index < manifest.entries.size(); ++index) {
        const auto& entry = manifest.entries[index];
        OrderedTrackTaskRecord record{};
        record.manifest_entry_index = index;
        record.source_row = entry.track_object_row;
        record.configured_car_index = entry.configured_car_index;
        record.record_tag = entry.record_tag;
        record.cursor_offset = entry.cursor_offset;

        if (entry.track_object_row.has_value()) {
            const auto source = std::find_if(materialization.rows.begin(),
                materialization.rows.end(), [&](const auto& row) {
                    return row.source_row == *entry.track_object_row;
                });
            if (source != materialization.rows.end()) {
                record.source_status = source->status;
                record.record_bytes = source->record_bytes;
                const bool complete_bytes =
                    source->status == TrackObjectTaskRecordStatus::complete_record &&
                    source->record_tag == entry.record_tag &&
                    source->record_bytes.size() >= 8U &&
                    read_u32_le(source->record_bytes, 0U) == entry.record_tag &&
                    read_u32_le(source->record_bytes, 4U) == source->record_bytes.size();
                if (!complete_bytes) {
                    result.complete = false;
                    if (!result.failed_manifest_entry.has_value())
                        result.failed_manifest_entry = index;
                }
            } else {
                record.source_status = TrackObjectTaskRecordStatus::incomplete_track_data;
                result.complete = false;
                if (!result.failed_manifest_entry.has_value())
                    result.failed_manifest_entry = index;
            }
        } else {
            // The native post-directive configured-car pass has an identity
            // and cursor but no track.obt line from which to materialize bytes.
            record.source_status = TrackObjectTaskRecordStatus::car_record_deferred;
            result.complete = false;
            if (!result.failed_manifest_entry.has_value())
                result.failed_manifest_entry = index;
        }
        result.records.push_back(std::move(record));
    }

    return result;
}

} // namespace viper
