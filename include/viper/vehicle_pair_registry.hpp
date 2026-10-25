#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace viper {

inline constexpr std::size_t vehicle_pair_registry_layers = 3U;
inline constexpr std::size_t vehicle_pair_registry_record_bytes = 0x14U;
inline constexpr std::size_t vehicle_pair_registry_row_header_bytes = 0x10U;

struct VehiclePairRecord {
    std::int16_t endpoint_a_ordinal_0x00{};
    std::int16_t endpoint_b_ordinal_0x02{};
    float position_delta_x_0x04{};
    float position_delta_z_0x08{};
    float velocity_delta_x_0x0c{};
    float velocity_delta_z_0x10{};
};

static_assert(sizeof(VehiclePairRecord) == vehicle_pair_registry_record_bytes);
static_assert(offsetof(VehiclePairRecord, position_delta_x_0x04) == 0x04U);
static_assert(offsetof(VehiclePairRecord, velocity_delta_x_0x0c) == 0x0cU);

struct VehiclePairParticipantRow {
    bool registered{};
    float position_x_0x5c{};
    float position_z_0x64{};
    float velocity_x_0x234{};
    float velocity_z_0x23c{};
};

struct VehiclePairRegistry {
    std::uint32_t vehicle_count{};
    std::uint32_t row_stride_bytes{};
    std::vector<VehiclePairRecord> records;
    // Row-major storage mirrors the native allocation at +0x10. Four DWORDs
    // precede each row's three N-wide pointer blocks in the native layout.
    std::vector<std::optional<std::size_t>> table_entries;

    [[nodiscard]] std::optional<std::size_t> pair_record_index(
        std::size_t layer, std::size_t row, std::size_t column) const noexcept;

    [[nodiscard]] std::optional<std::array<float, 2>> oriented_position_delta(
        std::size_t row, std::size_t column) const noexcept;
    [[nodiscard]] std::optional<std::array<float, 2>> oriented_velocity_delta(
        std::size_t row, std::size_t column) const noexcept;

    // Models the 0x41EC40 pair-difference writes after ordinal rows have been
    // populated by 0x419E80/0x41EC00. It does not run either native row sort.
    [[nodiscard]] bool update_pair_deltas(
        std::span<const VehiclePairParticipantRow> rows) noexcept;

    [[nodiscard]] std::optional<std::size_t> native_table_byte_offset(
        std::size_t layer, std::size_t row, std::size_t column) const noexcept;
};

// Reconstructs the bounded 0x41e9b0/0x41eab0 allocation and pair-table
// initialization. Pair records start with zeroed values, as in 0x41f190.
[[nodiscard]] std::optional<VehiclePairRegistry> make_vehicle_pair_registry(
    std::uint32_t vehicle_count);

} // namespace viper
