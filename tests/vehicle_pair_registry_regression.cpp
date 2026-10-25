#include "viper/vehicle_pair_registry.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {

void require(const bool condition, const char* const message)
{
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main()
{
    try {
        const auto empty = viper::make_vehicle_pair_registry(0U);
        require(empty.has_value() && empty->row_stride_bytes == 0x10U &&
                empty->records.empty() && empty->table_entries.empty(),
            "zero vehicle registry must preserve the native header stride");

        const auto single = viper::make_vehicle_pair_registry(1U);
        require(single.has_value() && single->records.empty() &&
                single->row_stride_bytes == 0x1cU &&
                !single->pair_record_index(0U, 0U, 0U).has_value() &&
                single->native_table_byte_offset(2U, 0U, 0U) == 0x18U,
            "single vehicle registry must have null diagonal slots");

        auto registry = viper::make_vehicle_pair_registry(3U);
        require(registry.has_value(), "three vehicle registry must initialize");
        require(registry->row_stride_bytes == 0x34U &&
                registry->records.size() == 3U &&
                registry->table_entries.size() == 3U * (3U * 3U + 4U),
            "registry allocation dimensions must match N choose 2 and native stride");
        require(registry->records[0].endpoint_a_ordinal_0x00 == 0 &&
                registry->records[0].endpoint_b_ordinal_0x02 == 1 &&
                registry->records[1].endpoint_a_ordinal_0x00 == 0 &&
                registry->records[1].endpoint_b_ordinal_0x02 == 2 &&
                registry->records[2].endpoint_a_ordinal_0x00 == 1 &&
                registry->records[2].endpoint_b_ordinal_0x02 == 2,
            "pair records must be initialized in lexicographic ordinal order");

        for (std::size_t layer = 0U;
             layer < viper::vehicle_pair_registry_layers; ++layer) {
            require(registry->pair_record_index(layer, 0U, 1U) == 0U &&
                    registry->pair_record_index(layer, 1U, 0U) == 0U &&
                    registry->pair_record_index(layer, 0U, 2U) == 1U &&
                    registry->pair_record_index(layer, 2U, 0U) == 1U &&
                    registry->pair_record_index(layer, 1U, 2U) == 2U &&
                    registry->pair_record_index(layer, 2U, 1U) == 2U &&
                    !registry->pair_record_index(layer, 1U, 1U).has_value(),
                "each table layer must share pair pointers in both directions");
        }

        require(registry->native_table_byte_offset(0U, 0U, 1U) == 0x14U &&
                registry->native_table_byte_offset(1U, 0U, 1U) == 0x20U &&
                registry->native_table_byte_offset(2U, 0U, 1U) == 0x2cU &&
                registry->native_table_byte_offset(2U, 1U, 0U) == 0x5cU,
            "reported table offsets must follow the native row and layer layout");
        require(!registry->native_table_byte_offset(3U, 0U, 1U).has_value() &&
                !registry->native_table_byte_offset(0U, 3U, 1U).has_value(),
            "invalid layer and ordinal lookups must be rejected");

        const std::array<viper::VehiclePairParticipantRow, 3> rows{{
            {true, 1.0F, 2.0F, 3.0F, 4.0F},
            {true, 8.0F, -2.0F, -1.0F, 6.0F},
            {false, 99.0F, 99.0F, 99.0F, 99.0F}}};
        require(registry->update_pair_deltas(rows),
            "all pair deltas must be produced from ordinal participant rows");
        const auto forward_position = registry->oriented_position_delta(0U, 1U);
        const auto reverse_position = registry->oriented_position_delta(1U, 0U);
        const auto forward_velocity = registry->oriented_velocity_delta(0U, 1U);
        const auto reverse_velocity = registry->oriented_velocity_delta(1U, 0U);
        require(forward_position == std::array<float, 2>{7.0F, -4.0F} &&
                reverse_position == std::array<float, 2>{-7.0F, 4.0F} &&
                forward_velocity == std::array<float, 2>{-4.0F, 2.0F} &&
                reverse_velocity == std::array<float, 2>{4.0F, -2.0F},
            "pair producer and accessors must preserve native position/velocity ordering and orientation");
        require(registry->records[1].position_delta_x_0x04 == -1.0F &&
                registry->records[1].position_delta_z_0x08 == -2.0F &&
                registry->records[1].velocity_delta_x_0x0c == -3.0F &&
                registry->records[1].velocity_delta_z_0x10 == -4.0F,
            "unregistered participant rows must remain native zero values during pair generation");
        const auto original = registry->records[0];
        const std::array<viper::VehiclePairParticipantRow, 2> short_rows{};
        require(!registry->update_pair_deltas(short_rows) &&
                registry->records[0].position_delta_x_0x04 ==
                    original.position_delta_x_0x04 &&
                registry->records[0].velocity_delta_x_0x0c ==
                    original.velocity_delta_x_0x0c,
            "mismatched row count must fail without partially updating pair records");
        require(!registry->oriented_position_delta(0U, 0U).has_value() &&
                !registry->oriented_velocity_delta(3U, 0U).has_value(),
            "oriented reads must reject diagonal and out-of-range pairs");

        require(!viper::make_vehicle_pair_registry(32769U).has_value(),
            "record ordinals outside the native signed 16-bit range must be rejected");
        std::cout << "vehicle pair registry regressions passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "vehicle pair registry regression failed: "
                  << error.what() << '\n';
        return 1;
    }
}
