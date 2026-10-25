#include "viper/vehicle_pair_registry.hpp"

#include <limits>

namespace viper {
namespace {

constexpr std::size_t max_vehicle_count_for_signed_ordinals =
    static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()) + 1U;

[[nodiscard]] std::optional<std::size_t> table_word_index(
    const VehiclePairRegistry& registry,
    const std::size_t layer,
    const std::size_t row,
    const std::size_t column) noexcept
{
    if (layer >= vehicle_pair_registry_layers ||
        row >= registry.vehicle_count || column >= registry.vehicle_count) {
        return std::nullopt;
    }
    const auto words_per_row = static_cast<std::size_t>(registry.vehicle_count) *
        vehicle_pair_registry_layers +
        vehicle_pair_registry_row_header_bytes / sizeof(std::uint32_t);
    const auto index = row * words_per_row +
        vehicle_pair_registry_row_header_bytes / sizeof(std::uint32_t) +
        layer * registry.vehicle_count + column;
    if (index >= registry.table_entries.size()) return std::nullopt;
    return index;
}

} // namespace

std::optional<std::size_t> VehiclePairRegistry::pair_record_index(
    const std::size_t layer,
    const std::size_t row,
    const std::size_t column) const noexcept
{
    const auto index = table_word_index(*this, layer, row, column);
    if (!index.has_value()) return std::nullopt;
    return table_entries[*index];
}

std::optional<std::array<float, 2>> VehiclePairRegistry::oriented_position_delta(
    const std::size_t row,
    const std::size_t column) const noexcept
{
    const auto record_index = pair_record_index(0U, row, column);
    if (!record_index.has_value() || *record_index >= records.size())
        return std::nullopt;
    const auto& record = records[*record_index];
    if (row == static_cast<std::size_t>(record.endpoint_a_ordinal_0x00) &&
        column == static_cast<std::size_t>(record.endpoint_b_ordinal_0x02)) {
        return std::array<float, 2>{record.position_delta_x_0x04,
            record.position_delta_z_0x08};
    }
    if (row != static_cast<std::size_t>(record.endpoint_b_ordinal_0x02) ||
        column != static_cast<std::size_t>(record.endpoint_a_ordinal_0x00))
        return std::nullopt;
    return std::array<float, 2>{-record.position_delta_x_0x04,
        -record.position_delta_z_0x08};
}

std::optional<std::array<float, 2>> VehiclePairRegistry::oriented_velocity_delta(
    const std::size_t row,
    const std::size_t column) const noexcept
{
    const auto record_index = pair_record_index(0U, row, column);
    if (!record_index.has_value() || *record_index >= records.size())
        return std::nullopt;
    const auto& record = records[*record_index];
    if (row == static_cast<std::size_t>(record.endpoint_a_ordinal_0x00) &&
        column == static_cast<std::size_t>(record.endpoint_b_ordinal_0x02)) {
        return std::array<float, 2>{record.velocity_delta_x_0x0c,
            record.velocity_delta_z_0x10};
    }
    if (row != static_cast<std::size_t>(record.endpoint_b_ordinal_0x02) ||
        column != static_cast<std::size_t>(record.endpoint_a_ordinal_0x00))
        return std::nullopt;
    return std::array<float, 2>{-record.velocity_delta_x_0x0c,
        -record.velocity_delta_z_0x10};
}

bool VehiclePairRegistry::update_pair_deltas(
    const std::span<const VehiclePairParticipantRow> rows) noexcept
{
    if (rows.size() != vehicle_count ||
        rows.size() > max_vehicle_count_for_signed_ordinals) {
        return false;
    }
    for (const auto& record : records) {
        const auto endpoint_a = static_cast<std::size_t>(
            record.endpoint_a_ordinal_0x00);
        const auto endpoint_b = static_cast<std::size_t>(
            record.endpoint_b_ordinal_0x02);
        if (endpoint_a >= rows.size() || endpoint_b >= rows.size() ||
            endpoint_a >= endpoint_b) {
            return false;
        }
    }
    const auto read_row = [rows](const std::size_t ordinal) {
        const auto& row = rows[ordinal];
        if (!row.registered) return VehiclePairParticipantRow{};
        return row;
    };
    for (auto& record : records) {
        const auto endpoint_a = static_cast<std::size_t>(
            record.endpoint_a_ordinal_0x00);
        const auto endpoint_b = static_cast<std::size_t>(
            record.endpoint_b_ordinal_0x02);
        const auto a = read_row(endpoint_a);
        const auto b = read_row(endpoint_b);
        record.position_delta_x_0x04 =
            b.position_x_0x5c - a.position_x_0x5c;
        record.position_delta_z_0x08 =
            b.position_z_0x64 - a.position_z_0x64;
        record.velocity_delta_x_0x0c =
            b.velocity_x_0x234 - a.velocity_x_0x234;
        record.velocity_delta_z_0x10 =
            b.velocity_z_0x23c - a.velocity_z_0x23c;
    }
    return true;
}

std::optional<std::size_t> VehiclePairRegistry::native_table_byte_offset(
    const std::size_t layer,
    const std::size_t row,
    const std::size_t column) const noexcept
{
    if (!pair_record_index(layer, row, column).has_value()) {
        // A diagonal slot is a valid null pointer, so distinguish it from an
        // invalid query using the bounded table-index calculation.
        if (!table_word_index(*this, layer, row, column).has_value())
            return std::nullopt;
    }
    const auto offset = static_cast<std::size_t>(row) * row_stride_bytes +
        vehicle_pair_registry_row_header_bytes +
        (layer * static_cast<std::size_t>(vehicle_count) + column) *
            sizeof(std::uint32_t);
    return offset;
}

std::optional<VehiclePairRegistry> make_vehicle_pair_registry(
    const std::uint32_t vehicle_count)
{
    if (vehicle_count > max_vehicle_count_for_signed_ordinals)
        return std::nullopt;

    const auto count = static_cast<std::size_t>(vehicle_count);
    const auto max_size = std::numeric_limits<std::size_t>::max();
    if (count != 0U && count >
            (std::numeric_limits<std::uint32_t>::max() -
             vehicle_pair_registry_row_header_bytes) /
                (vehicle_pair_registry_layers * sizeof(std::uint32_t))) {
        return std::nullopt;
    }
    const auto words_per_row = count * vehicle_pair_registry_layers +
        vehicle_pair_registry_row_header_bytes / sizeof(std::uint32_t);
    if (count != 0U && words_per_row > max_size / count) return std::nullopt;
    if (count > 1U && count > max_size / (count - 1U)) return std::nullopt;
    const auto record_count = count * (count - (count != 0U ? 1U : 0U)) / 2U;
    if (record_count > std::vector<VehiclePairRecord>{}.max_size() ||
        count * words_per_row > std::vector<std::optional<std::size_t>>{}.max_size()) {
        return std::nullopt;
    }

    VehiclePairRegistry registry{};
    registry.vehicle_count = vehicle_count;
    registry.row_stride_bytes = static_cast<std::uint32_t>(
        count * vehicle_pair_registry_layers * sizeof(std::uint32_t) +
        vehicle_pair_registry_row_header_bytes);
    registry.records.reserve(record_count);
    registry.table_entries.resize(count * words_per_row);

    for (std::size_t a = 0U; a < count; ++a) {
        for (std::size_t b = a + 1U; b < count; ++b) {
            const auto record_index = registry.records.size();
            registry.records.push_back(VehiclePairRecord{
                static_cast<std::int16_t>(a), static_cast<std::int16_t>(b),
                0.0F, 0.0F, 0.0F, 0.0F});
            for (std::size_t layer = 0U;
                 layer < vehicle_pair_registry_layers; ++layer) {
                const auto ab = table_word_index(registry, layer, a, b);
                const auto ba = table_word_index(registry, layer, b, a);
                if (!ab.has_value() || !ba.has_value()) return std::nullopt;
                registry.table_entries[*ab] = record_index;
                registry.table_entries[*ba] = record_index;
            }
        }
    }
    return registry;
}

} // namespace viper
