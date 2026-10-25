#include "viper/vehicle_pair_registry.hpp"

#include <limits>

namespace viper {
namespace {

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

std::optional<std::array<float, 2>> VehiclePairRegistry::oriented_value_a(
    const std::size_t row,
    const std::size_t column) const noexcept
{
    const auto record_index = pair_record_index(0U, row, column);
    if (!record_index.has_value() || *record_index >= records.size())
        return std::nullopt;
    const auto& record = records[*record_index];
    if (row == record.ordinal_a && column == record.ordinal_b)
        return record.value_a;
    if (row != record.ordinal_b || column != record.ordinal_a)
        return std::nullopt;
    return std::array<float, 2>{-record.value_a[0], -record.value_a[1]};
}

std::optional<std::array<float, 2>> VehiclePairRegistry::oriented_value_b(
    const std::size_t row,
    const std::size_t column) const noexcept
{
    const auto record_index = pair_record_index(0U, row, column);
    if (!record_index.has_value() || *record_index >= records.size())
        return std::nullopt;
    const auto& record = records[*record_index];
    if (row == record.ordinal_a && column == record.ordinal_b)
        return record.value_b;
    if (row != record.ordinal_b || column != record.ordinal_a)
        return std::nullopt;
    return std::array<float, 2>{-record.value_b[0], -record.value_b[1]};
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
    if (vehicle_count > std::numeric_limits<std::uint16_t>::max())
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
                static_cast<std::uint16_t>(a), static_cast<std::uint16_t>(b),
                {}, {}});
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
