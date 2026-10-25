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
        require(registry->records[0].ordinal_a == 0U &&
                registry->records[0].ordinal_b == 1U &&
                registry->records[1].ordinal_a == 0U &&
                registry->records[1].ordinal_b == 2U &&
                registry->records[2].ordinal_a == 1U &&
                registry->records[2].ordinal_b == 2U,
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

        registry->records[0].value_a = {12.5F, -3.0F};
        registry->records[0].value_b = {-4.0F, 8.25F};
        const auto forward_a = registry->oriented_value_a(0U, 1U);
        const auto reverse_a = registry->oriented_value_a(1U, 0U);
        const auto forward_b = registry->oriented_value_b(0U, 1U);
        const auto reverse_b = registry->oriented_value_b(1U, 0U);
        require(forward_a == std::array<float, 2>{12.5F, -3.0F} &&
                reverse_a == std::array<float, 2>{-12.5F, 3.0F} &&
                forward_b == std::array<float, 2>{-4.0F, 8.25F} &&
                reverse_b == std::array<float, 2>{4.0F, -8.25F},
            "pair accessors must reverse both planar components for the opposite ordinal");
        require(!registry->oriented_value_a(0U, 0U).has_value() &&
                !registry->oriented_value_b(3U, 0U).has_value(),
            "oriented reads must reject diagonal and out-of-range pairs");

        require(!viper::make_vehicle_pair_registry(65536U).has_value(),
            "record ordinals wider than the native 16-bit fields must be rejected");
        std::cout << "vehicle pair registry regressions passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "vehicle pair registry regression failed: "
                  << error.what() << '\n';
        return 1;
    }
}
