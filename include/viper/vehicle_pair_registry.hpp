#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace viper {

inline constexpr std::size_t vehicle_pair_registry_layers = 3U;
inline constexpr std::size_t vehicle_pair_registry_record_bytes = 0x14U;
inline constexpr std::size_t vehicle_pair_registry_row_header_bytes = 0x10U;

struct VehiclePairRecord {
    std::uint16_t ordinal_a{};
    std::uint16_t ordinal_b{};
    // Two oriented planar values. Static consumers use these as relative
    // velocity and relative position, respectively; their coordinate basis
    // and producer are not yet established.
    std::array<float, 2> value_a{};
    std::array<float, 2> value_b{};
};

static_assert(sizeof(VehiclePairRecord) == vehicle_pair_registry_record_bytes);
static_assert(offsetof(VehiclePairRecord, value_a) == 0x04U);
static_assert(offsetof(VehiclePairRecord, value_b) == 0x0cU);

struct VehiclePairRegistry {
    std::uint32_t vehicle_count{};
    std::uint32_t row_stride_bytes{};
    std::vector<VehiclePairRecord> records;
    // Row-major storage mirrors the native allocation at +0x10. Four DWORDs
    // precede each row's three N-wide pointer blocks in the native layout.
    std::vector<std::optional<std::size_t>> table_entries;

    [[nodiscard]] std::optional<std::size_t> pair_record_index(
        std::size_t layer, std::size_t row, std::size_t column) const noexcept;

    [[nodiscard]] std::optional<std::array<float, 2>> oriented_value_a(
        std::size_t row, std::size_t column) const noexcept;
    [[nodiscard]] std::optional<std::array<float, 2>> oriented_value_b(
        std::size_t row, std::size_t column) const noexcept;

    [[nodiscard]] std::optional<std::size_t> native_table_byte_offset(
        std::size_t layer, std::size_t row, std::size_t column) const noexcept;
};

// Reconstructs the bounded 0x41e9b0/0x41eab0 allocation and pair-table
// initialization. Pair records start with zeroed values, as in 0x41f190.
[[nodiscard]] std::optional<VehiclePairRegistry> make_vehicle_pair_registry(
    std::uint32_t vehicle_count);

} // namespace viper
