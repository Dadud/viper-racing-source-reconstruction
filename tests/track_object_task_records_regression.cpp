#include "viper/track_object_task_records.hpp"
#include "viper/model_resource.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void write_u32(std::vector<std::byte>& bytes, const std::size_t offset,
               const std::uint32_t value)
{
    for (std::size_t byte = 0; byte < 4U; ++byte)
        bytes[offset + byte] = static_cast<std::byte>((value >> (byte * 8U)) & 0xffU);
}

std::uint32_t read_u32(const std::span<const std::byte> bytes,
                       const std::size_t offset)
{
    return std::to_integer<std::uint32_t>(bytes[offset]) |
        (std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U) |
        (std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U) |
        (std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

viper::TrackObjectTextTable make_table(const std::span<const std::string_view> lines)
{
    viper::TrackObjectTextTable table{};
    table.line_count = static_cast<std::uint32_t>(lines.size());
    table.line_stride = 128U;
    table.raw_payload.resize(table.header_size + lines.size() * table.line_stride);
    for (std::size_t row = 0; row < lines.size(); ++row) {
        const auto at = table.header_size + row * table.line_stride;
        for (std::size_t i = 0; i < lines[row].size(); ++i)
            table.raw_payload[at + i] = static_cast<std::byte>(lines[row][i]);
    }
    return table;
}

viper::ResourceBundle make_model_bundle()
{
    constexpr std::size_t model_size = 40U + 32U;
    constexpr std::size_t header_size = 16U;
    constexpr std::size_t entry_size = 36U;
    constexpr std::size_t chunk_header_size = 8U;
    const auto boundary = header_size + entry_size + chunk_header_size + model_size;
    std::vector<std::byte> bundle(boundary);
    write_u32(bundle, 0U, viper::resource_type_code('R', 'S', 'T', '0'));
    write_u32(bundle, 4U, 1U);
    write_u32(bundle, 8U, 1U);
    write_u32(bundle, 12U, static_cast<std::uint32_t>(boundary));
    constexpr std::string_view name = "box.mod";
    for (std::size_t i = 0; i < name.size(); ++i)
        bundle[header_size + i] = static_cast<std::byte>(name[i]);
    write_u32(bundle, header_size + 16U, viper::ModelResource::resource_type_code);
    write_u32(bundle, header_size + 20U, 1U);
    write_u32(bundle, header_size + 24U, static_cast<std::uint32_t>(model_size));
    const auto payload = header_size + entry_size + chunk_header_size;
    write_u32(bundle, payload, 1U); // one 32-byte vertex
    write_u32(bundle, payload + 40U, std::bit_cast<std::uint32_t>(-1.0F));
    write_u32(bundle, payload + 44U, std::bit_cast<std::uint32_t>(-2.0F));
    write_u32(bundle, payload + 48U, std::bit_cast<std::uint32_t>(-3.0F));
    return viper::ResourceBundle::parse(std::move(bundle));
}

void check_complete_world_rows()
{
    const std::array lines{
        std::string_view{"out\\track.txt"},
        std::string_view{"obj wobble pole 42"},
        std::string_view{"obj static box 1 2 3 0 0 0 4 5 6"},
        std::string_view{"obj obstacle cube box.mod 10,20:3 2"},
        std::string_view{"obj checkpoint finish 0,0 1,1"},
        std::string_view{"end"},
        std::string_view{"obj wobble flap 2"},
    };
    const auto table = make_table(lines);
    const viper::TerrainSurfaceResourcePayload terrain{};
    auto bundle = make_model_bundle();
    const std::array<const viper::ResourceBundle*, 1> bundles{&bundle};
    const auto materialized = viper::materialize_track_object_task_records(
        table, terrain, bundles, 0U, true, false);
    require(materialized.complete, "supported world object rows must materialize");
    require(materialized.stopped_at_end, "end directive must stop world parsing");
    require(materialized.rows.size() == 5U, "only source rows before and including end are visited");
    const std::array<std::uint32_t, 4> expected_tags{
        0x574F424CU, 0x53544154U, 0x4F425354U, 0x43484B50U};
    std::size_t record_index = 0U;
    for (const auto& row : materialized.rows) {
        if (row.status != viper::TrackObjectTaskRecordStatus::complete_record) continue;
        require(row.record_tag == expected_tags[record_index], "record order/tag mismatch");
        require(row.record_bytes.size() >= 8U, "complete task record must include its header");
        require(read_u32(row.record_bytes, 0U) == row.record_tag,
            "serialized tag must match materialization metadata");
        require(read_u32(row.record_bytes, 4U) == row.record_bytes.size(),
            "serialized size must cover the exact record byte span");
        ++record_index;
    }
    require(record_index == expected_tags.size(), "all four supported record classes must be emitted");

    const auto& wobble = materialized.rows[0].record_bytes;
    require(wobble.size() == 0x20U &&
            read_u32(wobble, 8U) == std::bit_cast<std::uint32_t>(10000.0F) &&
            read_u32(wobble, 24U) == 42U && read_u32(wobble, 28U) == 0U,
        "WOBL bytes must retain native fixed fields, parsed parameter, and pole type");
    const auto& static_record = materialized.rows[1].record_bytes;
    require(static_record.size() == 0x48U &&
            read_u32(static_record, 8U) == std::bit_cast<std::uint32_t>(1.0F) &&
            read_u32(static_record, 44U) == std::bit_cast<std::uint32_t>(1.0F) &&
            read_u32(static_record, 56U) == 0U,
        "STAT bytes must contain identity matrix and zero reserved DWORD");
    const auto& obstacle = materialized.rows[2].record_bytes;
    require(obstacle.size() == 0x38U &&
            read_u32(obstacle, 24U) == std::bit_cast<std::uint32_t>(10.0F) &&
            read_u32(obstacle, 28U) == std::bit_cast<std::uint32_t>(6.0F) &&
            read_u32(obstacle, 52U) == 1U,
        "OBST bytes must use parsed position, model bounds, terrain height, and type");
    const auto& checkpoint = materialized.rows[3].record_bytes;
    require(checkpoint.size() == 0x20U &&
            read_u32(checkpoint, 8U) == std::bit_cast<std::uint32_t>(0.0F) &&
            read_u32(checkpoint, 20U) == std::bit_cast<std::uint32_t>(1.0F),
        "CHKP bytes must include terrain-sampled XYZ endpoints");
}

void check_explicit_unresolved_routes()
{
    const std::array car_lines{std::string_view{"out\\track.txt"},
                               std::string_view{"obj car 0, 0"}};
    const viper::TerrainSurfaceResourcePayload terrain{};
    const std::span<const viper::ResourceBundle* const> no_bundles{};
    const auto car_result = viper::materialize_track_object_task_records(
        make_table(car_lines), terrain, no_bundles, 0U, true, false);
    require(!car_result.complete && car_result.rows.size() == 1U &&
            car_result.rows[0].status == viper::TrackObjectTaskRecordStatus::car_record_deferred,
        "car rows must be explicitly deferred to the separate car producer");

    const std::array static_lines{std::string_view{"out\\track.txt"},
        std::string_view{"obj static box 1 2 3 0 0 0 4 5 6"}};
    const auto static_result = viper::materialize_track_object_task_records(
        make_table(static_lines), terrain, no_bundles, 0U, false, false);
    require(static_result.complete && static_result.rows[0].status ==
            viper::TrackObjectTaskRecordStatus::skipped_by_native_gate,
        "disabled native static gate must skip rather than fabricate STAT bytes");

    const std::array visual_checkpoint_lines{std::string_view{"out\\track.txt"},
        std::string_view{"obj checkpoint finish 0,0 1,1"}};
    const auto visual_checkpoint = viper::materialize_track_object_task_records(
        make_table(visual_checkpoint_lines), terrain, no_bundles, 1U, true, false);
    require(visual_checkpoint.complete && visual_checkpoint.rows[0].status ==
            viper::TrackObjectTaskRecordStatus::visual_only,
        "nonzero native track selector must keep checkpoint on its visual route");

    const std::array missing_model_lines{std::string_view{"out\\track.txt"},
        std::string_view{"obj obstacle cube absent.mod 1,2:3 4"}};
    const auto missing_model = viper::materialize_track_object_task_records(
        make_table(missing_model_lines), terrain, no_bundles, 0U, true, false);
    require(!missing_model.complete && missing_model.rows[0].status ==
            viper::TrackObjectTaskRecordStatus::missing_model_resource,
        "obstacle row without a resolved model must remain explicitly incomplete");

    const std::array unknown_lines{std::string_view{"out\\track.txt"},
        std::string_view{"obj ball 1,2"}};
    const auto unknown_result = viper::materialize_track_object_task_records(
        make_table(unknown_lines), terrain, no_bundles, 0U, true, false);
    require(!unknown_result.complete && unknown_result.rows.size() == 1U &&
            unknown_result.rows[0].status ==
                viper::TrackObjectTaskRecordStatus::unsupported_object_type,
        "unsupported object class must stop materialization explicitly");
}

} // namespace

int main()
{
    try {
        check_complete_world_rows();
        check_explicit_unresolved_routes();
        std::cout << "track object task record materialization checks passed\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
